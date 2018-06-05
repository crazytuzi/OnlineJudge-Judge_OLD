#include "argtable3.h"//命令行下对应的- ,解释不清楚
#include "runner.h"

#define INT_PLACE_HOLDER "<n>"
#define STR_PLACE_HOLDER "<str>"


/*
前面两个是啥
新增加了*answer_path 答案路径
*/
struct arg_lit *verb, *help, *version;
struct arg_int *max_cpu_time, *max_real_time, *max_memory, *max_stack,
        *max_process_number, *max_output_size, *uid, *gid;
struct arg_str *exe_path, *input_path, *output_path, *error_path,*answer_path,*args, *env, *log_path, *seccomp_rule_name;
struct arg_end *end;

int main(int argc, char *argv[]) {
    //命令行数组

    /*
    这里新增加了answer_path = arg_strn(NULL, "answer_path", STR_PLACE_HOLDER, 0, 1, "Answer Path")
    */
    void *arg_table[] = {
            help = arg_litn(NULL, "help", 0, 1, "Display This Help And Exit"),
            version = arg_litn(NULL, "version", 0, 1, "Display Version Info And Exit"),
            max_cpu_time = arg_intn(NULL, "max_cpu_time", INT_PLACE_HOLDER, 0, 1, "Max CPU Time (ms)"),
            max_real_time = arg_intn(NULL, "max_real_time", INT_PLACE_HOLDER, 0, 1, "Max Real Time (ms)"),
            max_memory = arg_intn(NULL, "max_memory", INT_PLACE_HOLDER, 0, 1, "Max Memory (byte)"),
            max_stack = arg_intn(NULL, "max_stack", INT_PLACE_HOLDER, 0, 1, "Max Stack (byte, default 16M)"),
            max_process_number = arg_intn(NULL, "max_process_number", INT_PLACE_HOLDER, 0, 1, "Max Process Number"),
            max_output_size = arg_intn(NULL, "max_output_size", INT_PLACE_HOLDER, 0, 1, "Max Output Size (byte)"),

            exe_path = arg_str1(NULL, "exe_path", STR_PLACE_HOLDER, "Exe Path"),
            input_path = arg_strn(NULL, "input_path", STR_PLACE_HOLDER, 0, 1, "Input Path"),
            output_path = arg_strn(NULL, "output_path", STR_PLACE_HOLDER, 0, 1, "Output Path"),
            error_path = arg_strn(NULL, "error_path", STR_PLACE_HOLDER, 0, 1, "Error Path"),
            answer_path = arg_strn(NULL, "answer_path", STR_PLACE_HOLDER, 0, 1, "Answer Path"),
            args = arg_strn(NULL, "args", STR_PLACE_HOLDER, 0, 255, "Arg"),
            env = arg_strn(NULL, "env", STR_PLACE_HOLDER, 0, 255, "Env"),

            log_path = arg_strn(NULL, "log_path", STR_PLACE_HOLDER, 0, 1, "Log Path"),
            seccomp_rule_name = arg_strn(NULL, "seccomp_rule_name", STR_PLACE_HOLDER, 0, 1, "Seccomp Rule Name"),

            uid = arg_intn(NULL, "uid", INT_PLACE_HOLDER, 0, 1, "UID (default 65534)"),
            gid = arg_intn(NULL, "gid", INT_PLACE_HOLDER, 0, 1, "GID (default 65534)"),

            end = arg_end(10),
    };

    int exitcode = 0;
    char name[] = "libjudger.so";
    //把命令行的值赋值给arg_table
    int nerrors = arg_parse(argc, argv, arg_table);

    //查看帮助
    if (help->count > 0) {
        printf("Usage: %s", name);
        arg_print_syntax(stdout, arg_table, "\n\n");
        arg_print_glossary(stdout, arg_table, "  %-25s %s\n");
        goto exit;
    }
    //查看版本
    if (version->count > 0) {
        printf("Version: %d.%d.%d\n", (VERSION >> 16) & 0xff, (VERSION >> 8) & 0xff, VERSION & 0xff);
        goto exit;
    }
    //转换有错误
    if (nerrors > 0) {
        arg_print_errors(stdout, end, name);
        printf("Try '%s --help' for more information.\n", name);
        exitcode = 1;
        goto exit;
    }

    struct config _config;
    struct result _result = {0, 0, 0, 0, 0, 0, 0};

    if (max_cpu_time->count > 0) {
        _config.max_cpu_time = *max_cpu_time->ival;
    } else {
        _config.max_cpu_time = UNLIMITED;
    }

    if (max_real_time->count > 0) {
        _config.max_real_time = *max_real_time->ival;
    } else {
        _config.max_real_time = UNLIMITED;
    }

    if (max_memory->count > 0) {
        //先用int存放再转化long,如果存在的时候已经超过int怎么办
        //第一种,一般不会给一个超过int的值
        //第二种,这个arg_intn是代表整数类型?包括long和int?
        _config.max_memory = (long) *max_memory->ival;
    } else {
        _config.max_memory = UNLIMITED;
    }

    if (max_stack->count > 0) {
        _config.max_stack = (long) *max_stack->ival;
    } else {
        //默认大小和时间都给了无限,但是栈有限制,安全考略?
        _config.max_stack = 16 * 1024 * 1024;
    }

    if (max_process_number->count > 0) {
        _config.max_process_number = *max_process_number->ival;
    } else {
        _config.max_process_number = UNLIMITED;
    }

    if (max_output_size->count > 0) {
        _config.max_output_size = (long) *max_output_size->ival;
    } else {
        _config.max_output_size = UNLIMITED;
    }

    _config.exe_path = (char *)*exe_path->sval;

    if (input_path->count > 0) {
        //只取了0,意思是,只能一组in out?
        _config.input_path = (char *)input_path->sval[0];
    } else {
        //没有输入的情况?特殊数据?
        _config.input_path = "/dev/stdin";
    }
    if (output_path->count > 0) {
        _config.output_path = (char *)output_path->sval[0];
    } else {
        _config.output_path = "/dev/stdout";
    }
    if (error_path->count > 0) {
        _config.error_path = (char *)error_path->sval[0];
    } else {
        _config.error_path = "/dev/stderr";
    }

    //这里是新增加的,与其他区别,没有else
    if (answer_path->count > 0) {
        _config.answer_path = (char *)answer_path->sval[0];
    }
    //args是什么,和exe_path一样,args是命令行使用,exe_path是配置用?
    _config.args[0] = _config.exe_path;
    int i = 1;
    //args存放的是啥数据
    if (args->count > 0) {
        for (; i < args->count + 1; i++) {
            _config.args[i] = (char *)args->sval[i - 1];
        }
    }
    _config.args[i] = NULL;

    i = 0;
    //每次执行是一个程序,为什么获取多个的值
    //这里是总配置?还是这个配置不是每次都需要添加
    if (env->count > 0) {
        for (; i < env->count; i++) {
            _config.env[i] = (char *)env->sval[i];
        }
    }
    _config.env[i] = NULL;

    if (log_path->count > 0) {
        _config.log_path = (char *)log_path->sval[0];
    } else {
        _config.log_path = "judger.log";
    }
    //安全机制
    if (seccomp_rule_name->count > 0) {
        _config.seccomp_rule_name = (char *)seccomp_rule_name->sval[0];
    } else {
        _config.seccomp_rule_name = NULL;
    }

    if (uid->count > 0) {
        _config.uid = (uid_t)*(uid->ival);
    }
    else {
        _config.uid = 65534;
    }
    if(gid->count > 0) {
        _config.gid = (gid_t)*(gid->ival);
    }
    else {
        _config.gid = 65534;
    }

    run(&_config, &_result);

    printf("{\n"
           "    \"cpu_time\": %d,\n"
           "    \"real_time\": %d,\n"
           "    \"memory\": %ld,\n"
           "    \"signal\": %d,\n"
           "    \"exit_code\": %d,\n"
           "    \"error\": %d,\n"
           "    \"result\": %d\n"
           "}",
           _result.cpu_time,
           _result.real_time,
           _result.memory,
           _result.signal,
           _result.exit_code,
           _result.error,
           _result.result);

    exit:
    //释放内存
    arg_freetable(arg_table, sizeof(arg_table) / sizeof(arg_table[0]));
    return exitcode;
}
