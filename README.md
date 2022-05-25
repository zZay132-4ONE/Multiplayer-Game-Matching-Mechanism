# Linux基础课 - Thrift项目

> - **作者：** Zzay
>
> - **主题：** 基于 RPC 框架 Thrift 的游戏匹配池实现
>
> - **课程：** https://www.acwing.com/file_system/file/content/whole/index/content/2991899/

## 1.1 项目结构

- `match_server`： 匹配池服务模块的服务端。（https://git.acwing.com/Zzay/thrift_lesson/-/tree/master/match_system）

- `game`： 匹配池服务模块的客户端。（https://git.acwing.com/Zzay/thrift_lesson/-/tree/master/game）

- `thrift`： 存放自定义的 `.thrift` 文件。（https://git.acwing.com/Zzay/thrift_lesson/-/tree/master/thrift）


## 1.2 知识点总结


- Thrift 文件的编写（与 GRPC 十分类似）：

    ```thrift
    /** 设置对应语言的命名空间 */
    namespace cpp match_service

    /** 定义实体 */
    struct User {
        1: i32 id,
        2: string name,
        3: i32 score
    }
     
    /** 定义 service */
    service Match {
        i32 add_user(1: User user, 2: string info),

        i32 remove_user(1: User user, 2: string info),
    }
    ```

- 基于自定义 thrift 文件，选择某种语言，并自动生成对应的接口类：

    ```bash
    // 如基于 cpp 的 thrift 文件
    thrift -r --gen cpp /path/to/file/*.thrift

    // 如基于 python3 的 thrift 文件
    thrift -r --gen py /path/to/file/*.thrift
    ```

- 编译 & 链接文件：

    ```bash
    // 编译，生成 .o 文件
    g++ -c [FILENAME]

    // 链接 .o 文件。生成可执行文件
    g++ *.o -o [FILENAME]

    // 对于 thrift 文件，需要额外引入 -lthrift ，例子如下
    g++ *.o -o main -lthrift
    ```
