#!/usr/bin/env python
# -*- coding:utf8 -*-
# 导入redis模块，通过python操作redis 也可以直接在redis主机的服务端操作缓存数据库
import redis
import _judger

import os
import os.path
if os.system("gcc main.c -o main"):
    print("compile error")
    exit(1)


# host是redis主机，需要redis服务端和客户端都启动 redis默认端口是6379
pool_0=redis.ConnectionPool(host='localhost',port=6379,password='olily',db=0,decode_responses=True)
r_0=redis.Redis(connection_pool=pool_0)
keys=r_0.keys()


def run(file_path_conf='',input_path_conf='',answer_path_conf='',output_path_conf='',error_path_conf='',
        max_cpu_time_conf=1000,max_memory_conf=128):
    ret = _judger.run(max_cpu_time=max_cpu_time_conf,
                      max_real_time=max_cpu_time_conf*2,
                      max_memory=max_memory_conf * 1024 * 1024,
                      max_process_number=200,
                      max_output_size=10000,
                      max_stack=32 * 1024 * 1024,
                      # five args above can be _judger.UNLIMITED
                      exe_path='main',
                      input_path=input_path_conf,
                      output_path=output_path_conf,
                      error_path=error_path_conf,
                      answer_path=answer_path_conf,
                      args=[file_path_conf],
                      # can be empty list
                      env=[],
                      log_path="judger.log",
                      # can be None
                      seccomp_rule_name="c_cpp",
                      uid=0,
                      gid=0)
    print(ret)
    return ret

def judge(problemid,runfile_name,outputfile_name,errorfile_name,max_cpu_time,max_memory):
    path="/oj/problem/"+str(problemid)
    file_list=os.listdir(path)
    file_count=len(file_list)/2
    max_cpu_time_use=-1
    max_memory_use=-1
    for i in range(file_count):
        in_path=os.path.join(path,'%d.in'%i)
        out_path=os.path.join(path,'%d.out'%i)
        if os.path.isfile(in_path) and os.path.isfile(out_path):
            ret=run(file_path_conf=runfile_name,input_path_conf=in_path,answer_path_conf=out_path,
                    output_path_conf=outputfile_name,error_path_conf=errorfile_name,
                    max_cpu_time_conf=max_cpu_time,max_memory_conf=max_memory)
            result=ret['result']
            cpu_time = ret['cpu_time']
            memory = ret['memory']
            if result == 0:
                if cpu_time > max_cpu_time_use:
                    max_cpu_time_use=cpu_time
                if memory >max_memory_use:
                    max_memory_use=memory
            else:
                return result,cpu_time,memory
        else:
            pass
    return 0,max_cpu_time_use,max_memory


#WA -1 0
#AC 0 1
result_list=[
    'WA',
    'AC',
    'TLE',
    'RTLE',
    'MLE',
    'RE',
    'SE',
    'PE',
    'OLE'

]

for key in keys:
    config=r_0.hgetall(key)
    status = config['status']
    if status == '0':
        #r_0.hset(key,'status','1')
        runid=str(key)
        problemid=config['problemid']
        username=config['username']
        code = config['code']
        TimeLimit=int(config['TimeLimit'])
        MemoryLimit = int(config['MemoryLimit'])
        code_path = "/oj/code/"
        outputanderror_path="/oj/temp/"
        runfile_name_conf=code_path+runid
        runfile_name=code_path+runid+'.c'
        outputfile_name = outputanderror_path+runid + '.output'
        errorfile_name = outputanderror_path+runid + '.output'
        os.mknod(runfile_name)
        file=open(runfile_name,'w')
        file.write(code)
        file.close()
        result, cpu_time, memory=judge(problemid,runfile_name_conf,outputfile_name,errorfile_name,TimeLimit,MemoryLimit)
        r_0.delete(key)
        pool_3 = redis.ConnectionPool(host='localhost', port=6379, password='olily', db=3, decode_responses=True)
        r_3 = redis.Redis(connection_pool=pool_3)
        r_3.hset(key, 'result', result)
        r_3.hset(key, 'run-time', cpu_time)
        r_3.hset(key, 'run-memory', memory)


        pool_2 = redis.ConnectionPool(host='localhost', port=6379, password='olily', db=2, decode_responses=True)
        r_2 = redis.Redis(connection_pool=pool_2)
        hresult=r_2.hgetall(problemid)
        result_count=hresult[result_list[result+1]]
        r_2.hset(problemid,result_list[result+1],int(result_count)+1)
        if result == 0:
            pool_4 = redis.ConnectionPool(host='localhost', port=6379, password='olily', db=4, decode_responses=True)
            r_4 = redis.Redis(connection_pool=pool_4)
            r_4.sadd(username,problemid)
            pool_5 = redis.ConnectionPool(host='localhost', port=6379, password='olily', db=5, decode_responses=True)
            r_5 = redis.Redis(connection_pool=pool_5)
            r_5.srem(username, problemid)
            #need judge r.smembers? or dirctly r.srem
        else:
            pool_4 = redis.ConnectionPool(host='localhost', port=6379, password='olily', db=4, decode_responses=True)
            r_4 = redis.Redis(connection_pool=pool_4)
            members=r_4.smembers(username)
            if problemid not in members:
                pool_5 = redis.ConnectionPool(host='localhost', port=6379, password='olily', db=5, decode_responses=True)
                r_5 = redis.Redis(connection_pool=pool_5)
                r_5.sadd(username, problemid)