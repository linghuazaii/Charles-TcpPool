# Charles TCP Pool
Welcome to **Charles TCP Pool**, it is a simple self-repair tcp connection pool. And it would be very nice to extend it. I will be grateful.

# Usage
 - `git clone https://github.com/linghuazaii/Charles-TcpPool.git`
 - `make`
 - the shared library will be generated. and also the pool `test` and the simple test `server`

# Note
 - for `CharlesTcpPool::getConnection(timeout)`, this `timeout` is the timeout you wait for a valid connection and you can't ignore it, or you can just modify the code. this function will return `NULL` if it can't give you a valid connection. in this situation, you shouldn't use `putConnection()` to put this connection back since `NULL` is not a connection. and even though you get a connection not `NULL`, when you do `write` or `read` you may find that this connection is bad,but don't worry, just `putConnection()` to return to the pool and wait for some time or continue to try to `getConnection()` until your job is done. you better wait for some time, since my baby thread will repair the bad connection which might be caused by crashed server or server closes the connection.
 - for `TCP_KEEPALIVE`, this part is not integrated, you can extend it. some server may close connection if connection is not active for some time. but that's ok, if it is closed then it is repaired. if you use `TCP_KEEPALIVE`, this version will use system config, you can extend it to use your own `KEEPALIVE` config in progress scope. 
 - there is possibility that if you use blocking io, think about it, client do `write` then `read` and some bad server do `read`, `write` and then `close`. if this `close` packet isn't received by client after client do `getConnection` `write` `read` `getConnection`(get the same fd, but it is closed) `write` `read`, the last `read` will return `0`. and it is your responsibility to handle `client/server` logic, I ensure you this pool works fine.


