#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "EventLoop.h"
#include "Logger.h"
#include "RedisConnectionPool.h"

#include <iostream>
#include <map>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

// #define DEBUG

using json = nlohmann::json;

bool benchmark = false;

// 封装返回 HTML 的函数
void renderHomeHtml(const std::string &htmlPath, HttpResponse *resp, const std::string &messagesJson = "[]")
{
    // 读取 HTML 文件
    std::ifstream htmlFile(htmlPath);
    if (!htmlFile.is_open())
    {
      LOG_ERROR("Failed to open HTML file: %s", htmlPath.c_str());
      resp->setStatusCode(HttpResponse::k404NotFound);
      resp->setBody("404 Not Found: home.html not found 1");
      return;
    }

    std::stringstream buffer;
    buffer << htmlFile.rdbuf();
    std::string htmlContent = buffer.str();

    // 将消息 JSON 嵌入到 HTML 中
    const std::string marker = "</head>";
    size_t pos = htmlContent.find(marker);
    if (pos != std::string::npos)
    {
      std::string script = "<script>window.initMessages = " + messagesJson + ";</script>";
      htmlContent.insert(pos, script);
    }
    else
    {
      LOG_ERROR("%s:Failed to find </head> in HTML, messages not embedded", __FUNCTION__);
    }

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("text/html");
    resp->setBody(htmlContent);
}

// 格式化时间
time_t parseTimeString(const std::string& timeStr) {
    // 示例格式: "2023:10:01 10:00:00"
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y:%m:%d %H:%M:%S");
    if (ss.fail()) {
        return time(nullptr); // 解析失败返回当前时间
    }
    return std::mktime(&tm);
}

void handlePostRequest(const HttpRequest &req, HttpResponse *resp) {
    try {
        // 解析JSON请求体
        auto json_body = json::parse(req.body());

        // 验证必要字段
        if (!json_body.contains("name") || !json_body.contains("message") || !json_body.contains("time")) {
            LOG_ERROR("Missing required fields: name, message or time\n");
            resp->setStatusCode(HttpResponse::k400BadRequest);
            resp->setContentType("application/json");
            resp->setBody(json{
                {"status", "failed"},
                {"reason", "Missing required fields: name, message or time"}
            }.dump());
            return;
        }

        // 提取数据
        std::string name = json_body["name"].get<std::string>();
        std::string message = json_body["message"].get<std::string>();
        std::string time_str = json_body["time"].get<std::string>();
        time_t timestamp = parseTimeString(time_str);

        // 将timestamp，传入lua脚本用
        std::string timestamp_str = std::to_string(timestamp);

        #ifdef DEBUG
            std::cout << "Received message - "
                      << "Name: " << name << ", "
                      << "Message: " << message << ", "
                      << "Time: " << time_str << std::endl;
        #endif

        // 获取Redis连接
        RedisConnectionPool& redis_pool = RedisConnectionPool::getInstance();
        auto conn = redis_pool.getConnection();
        if (!conn) {
            LOG_ERROR("Failed to get Redis connection\n");
            resp->setStatusCode(HttpResponse::k500InternalServerError);
            resp->setContentType("application/json");
            resp->setBody(json{
                {"status", "failed"},
                {"reason", "Redis connection failed"}
            }.dump());
            return;
        }

        // 定义Lua脚本
        // KEYS: none (we're using ARGV only)
        // ARGV[1]: name
        // ARGV[2]: message
        // ARGV[3]: time string
        // ARGV[4]: timestamp
        const std::string lua_script = R"(
            -- 生成唯一ID
            local id = redis.call('INCR', 'message:id_counter')
            
            -- 构造消息ID
            local message_id = 'message:' .. id
            
            -- 存储消息详情
            redis.call('HSET', message_id,
                      'name', ARGV[1],
                      'message', ARGV[2],
                      'time', ARGV[3])
            
            -- 添加到时间排序的有序集合
            redis.call('ZADD', 'messages_by_time', ARGV[4], message_id)
            
            -- 返回消息ID
            return message_id
        )";

        // 执行Lua脚本
        redisReply* reply = (redisReply*)redisCommand(
            conn.get(),
            "EVAL %s 0 %b %b %b %b",
            lua_script.c_str(),
            name.data(), name.size(),
            message.data(), message.size(),
            time_str.data(), time_str.size(),
            timestamp_str.data(), timestamp_str.size());

        // 处理脚本执行结果
        if (!reply) {
            LOG_ERROR("Null reply from Redis\n");
            resp->setStatusCode(HttpResponse::k500InternalServerError);
            resp->setContentType("application/json");
            resp->setBody(json{
                {"status", "failed"},
                {"reason", "Redis command failed"}
            }.dump());
            redis_pool.releaseConnection(conn);
            return;
        }
            cout << "lua脚本执行结果1" << endl;
        if (reply->type == REDIS_REPLY_ERROR) {
            LOG_ERROR("Redis error: %s\n", reply->str);
            resp->setStatusCode(HttpResponse::k500InternalServerError);
            resp->setContentType("application/json");
            resp->setBody(json{
                {"status", "failed"},
                {"reason", reply->str}
            }.dump());
            freeReplyObject(reply);
            redis_pool.releaseConnection(conn);
            return;
        }
            cout << "lua脚本执行结果2" << endl;
        if (reply->type != REDIS_REPLY_STRING) {
            LOG_ERROR("Unexpected reply type: %d\n", reply->type);
            resp->setStatusCode(HttpResponse::k500InternalServerError);
            resp->setContentType("application/json");
            resp->setBody(json{
                {"status", "failed"},
                {"reason", "Unexpected response format"}
            }.dump());
            freeReplyObject(reply);
            redis_pool.releaseConnection(conn);
            return;
        }

        // 获取返回的消息ID
        std::string message_id(reply->str, reply->len);
        freeReplyObject(reply);

        // 释放连接
        redis_pool.releaseConnection(conn);

        // 返回成功响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "success"},
            {"message_id", message_id}
        }.dump());

    } catch (const std::exception& e) {
        LOG_ERROR("Exception: %s\n", e.what());
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"},
            {"reason", e.what()}
        }.dump());
    }
}

// 处理根路径GET请求
void handleRootRequest(const HttpRequest &req, HttpResponse *resp) {
    // 获取Redis连接
    RedisConnectionPool& redis_pool = RedisConnectionPool::getInstance();
    auto conn = redis_pool.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
    }

    // 获取按时间排序的留言ID列表
    redisReply* zrange_reply = (redisReply*)redisCommand(conn.get(), 
        "ZREVRANGE messages_by_time 0 -1 WITHSCORES");
    
    if (!zrange_reply || zrange_reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(zrange_reply);

        LOG_ERROR("Failed to retrieve message list\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
    }

    json messages = json::array();
    
    // 遍历所有留言ID，获取详情
    for (size_t i = 0; i < zrange_reply->elements; i += 2) {
        std::string message_id = zrange_reply->element[i]->str;
        time_t timestamp = std::stol(zrange_reply->element[i+1]->str);

        // 获取留言详情
        redisReply* hgetall_reply = (redisReply*)redisCommand(conn.get(), 
            "HGETALL %s", message_id.c_str());
        
        if (hgetall_reply && hgetall_reply->type == REDIS_REPLY_ARRAY) {
            json message;
            // message["id"] = message_id;
            // message["timestamp"] = timestamp;
            
            for (size_t j = 0; j < hgetall_reply->elements; j += 2) {
                std::string field = hgetall_reply->element[j]->str;
                std::string value = hgetall_reply->element[j+1]->str;
                message[field] = value;
            }
            
            messages.push_back(message);
        }
        freeReplyObject(hgetall_reply);
    }
    freeReplyObject(zrange_reply);

    // 释放连接回池中
    redis_pool.releaseConnection(conn);

#ifdef DEBUG
    std::cout << messages.dump() << std::endl;
#endif

    // 渲染HTML页面并嵌入留言数据
    renderHomeHtml("/home/cf/test_Projects/muduo/http/resource/home.html", 
                  resp, 
                  messages.dump());
}

void onRequest(const HttpRequest &req, HttpResponse *resp) {
    std::cout << "Headers " << req.methodString() << " " << req.path() << std::endl;
    
    if (!benchmark) {
        const std::map<string, string> &headers = req.headers();
        for (const auto &header : headers) {
            std::cout << header.first << ": " << header.second << std::endl;
        }
    }

    try {
        // 处理POST请求 - 提交留言
        if (req.method() == HttpRequest::kPost) {
            handlePostRequest(req, resp);
        }
        // 处理根路径请求 - 显示留言板
        else if (req.path() == "/") {
            handleRootRequest(req, resp);
        }
        // 示例hello端点
        else if (req.path() == "/hello") {
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("text/plain");
            resp->addHeader("Server", "Muduo");
            resp->setBody("hello, world!\n");
        }
        // 404处理
        else {
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Request handling error: %s", e.what());
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "error"},
            {"message", e.what()}
        }.dump());
    }
}

RedisConnectionPool& createRedisPool()
{
    try {
        auto& pool = RedisConnectionPool::getInstance();
        pool.init("127.0.0.1", 6379);
        return pool;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize Redis pool\n");
        LOG_FATAL("%s Failed to initialize Redis pool.\n", __FUNCTION__);
    }
}


int main(int argc, char *argv[])
{
    int numThreads = 0;
    if (argc > 1)
    {
      benchmark = true;
      numThreads = atoi(argv[1]);
    }

    RedisConnectionPool& redis_pool = createRedisPool();

    EventLoop loop;
    HttpServer server(&loop, InetAddress(8080), "dummy", TcpServer::kReusePort);
    server.setHttpCallback(onRequest);
    server.setThreadNum(numThreads);
    server.start();
    loop.loop();
}