#include "RedisConnectionPool.h"


RedisConnectionPool& RedisConnectionPool::getInstance()
{
    static RedisConnectionPool instance;
    return instance;
}

void RedisConnectionPool::init(const std::string& host, int port, 
                                const std::string& password, 
                                int db, 
                                int poolSize)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(initialized_){
        throw std::runtime_error("Redis pool is already created.\n");
    }

    host_ = host;
    port_ = port;
    password_ = password;
    db_ = db;
    poolSize_ = poolSize;

    for(int i = 0; i < poolSize_; i++){
        auto conn = createConnection();
        if (conn){
            connections_.push(conn);
        }
        else {
            throw std::runtime_error("Failed in create redis conn.\n");
        }

        initialized_ = true;
    }
}

std::shared_ptr<redisContext> RedisConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(mutex_);

    if(!initialized_){
        throw std::runtime_error("Redis pool is not created.\n");
    }

    // 当连接池为空并且当前连接数量达到连接池上限，等待连接释放
    while(connections_.empty() && currentSize_ >= poolSize_){
        condition_.wait(lock);
    }

    if(!connections_.empty()){
        auto conn = connections_.front();
        connections_.pop();
        return conn;
    }

    if(currentSize_ < poolSize_){
        auto conn = createConnection();
        if(conn){
            currentSize_++;
            return conn;
        }
    }

    throw std::runtime_error("Failed in getConnection().\n");
}

void RedisConnectionPool::releaseConnection(std::shared_ptr<redisContext> conn)
{
    if(!conn) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查连接是否有效
    if(conn->err){
        // 无效连接则删除
        redisFree(conn.get());
        currentSize_--;

        // 创建新连接替代无效连接
        auto newConn = createConnection();
        if(newConn){
            connections_.push(newConn);
        }
    }
    else{
        connections_.push(conn);
    }
    // 唤醒等待中的获取连接函数
    condition_.notify_one();
}

void RedisConnectionPool::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);

    while(!connections_.empty()){
        auto conn = connections_.front();
        redisFree(conn.get());
        currentSize_--;
    }

    initialized_ = false;
}

RedisConnectionPool::~RedisConnectionPool()
{
    shutdown();
}

std::shared_ptr<redisContext> RedisConnectionPool::createConnection()
{
    redisContext * context = redisConnect(host_.c_str(), port_);
    if(!context || context->err){
        if(context){
            redisFree(context);
        }
        return nullptr;
    }

    if(!password_.empty()){
        redisReply * reply = (redisReply*)redisCommand(context, "AUTH %s", password_.c_str());
        if(!reply || reply->type == REDIS_REPLY_ERROR){
            freeReplyObject(reply);
            redisFree(context);
            return nullptr;
        }
        freeReplyObject(reply);
    }

    if(db_ != 0){
        redisReply * reply = (redisReply*)redisCommand(context, "SELECT %d", db_);
        if(!reply || reply->type == REDIS_REPLY_ERROR){
            freeReplyObject(reply);
            redisFree(context);
            return nullptr;
        }
        freeReplyObject(reply);
    }

    // 使用shared_ptr的自定义删除器，确保正确释放连接
    return std::shared_ptr<redisContext>(context, [this](redisContext* ctx) {
        std::lock_guard<std::mutex> lock(mutex_);
        redisFree(ctx);
        currentSize_--;
    });
}