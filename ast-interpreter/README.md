这是一个AST解释器，可以读取伪代码进行编译运行

评测流程简介：
1.在源码目录中创建build目录
2.在build目录中运行cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. && cmake --build .进行编译
3.编译成功后，使用cmake --install .安装
4.使用命令行运行程序（比如第一次作业就是ast-interpreter '输入程序'，具体每次作业的输入输出方法请参考作业给出的模板）
5.将程序的输出与标准答案进行比对
三次作业都将遵从类似的模式


提交作业时的要求：
1.打包整个项目为tar格式，可以是tar.gz，tar.xz等进一步压缩的tar
2.压缩包里不要有多余目录，要求解压出来直接是项目文件，而不是有一个子文件夹
3.压缩包要包含完整的项目，解压后可以直接运行cmake
4.压缩包不要包含自行编译的程序，节省空间
5.编译、运行环境均以作业1给出的docker环境为准，如果程序不能在该环境成功编译运行，就视为零分
7.编译时使用debug模式编译，所以记得删掉调试输出
8.评测时将会把标准输出和标准错误流同时重定向到文件，所以输出答案时可以输出到任意的流，但不推荐混用，因为标准输出流有缓冲区而错误流没有，需要手动flush，否则会导致答案错误
9.部分同学在Mac下打包出来的压缩包不符合常规格式，如果你不知道如何在Mac下正确打包，请在Linux或Windows下打包
10.作业提供的模板大方向（如输入输出格式）是正确的，题目描述不清的时候大家可以参考一下模板
11.作业提供的模板是在旧版LLVM下测试通过的，在LLVM20下不保证编译通过，可能需要一些调整
12.作业模板中的CMakelists.txt如存在“NO_DEFAULT_PATH”，需要去除


勘误：
○ 作业二：test14某行答案应该是“32 : minus, plus”，使用更精确的方法算出“32 : plus”也正确。


作业提交方法：
○ 使用国科大的邮箱（后缀必须是@mails.ucas.ac.cn）发送邮件至bypag@ict.ac.cn
○ 邮件标题写“[编译程序高级教程作业提交] Assignment 1”（同理，第二第三次作业把阿拉伯数字改成2或3，注意开头不要有多余空格）
○ 把作业打包添加为普通附件，文件名只要求扩展名正确（*.tar或*.tar.*），邮件正文内容随意，附件大小要求2KB~512KB
○ 若提交成功，你将收到邮件回复，评测系统将自动排队测试你的作业的正确性，测试完成后的结果也将通过邮件回复
○ 评测系统每5分钟收取一次邮件，同学们发送邮件后请耐心等待
○ 每人限制成功提交的次数为10次，该次数仅为防止攻击而设定，原则上不限次数，如果超过了请联系助教放宽限制
○ 每个测试点通过得1分，全部通过则为满分，每次作业的满分不一定相同
○ 评测本身没有时间限制，只有次数限制，我们记录评测结果时会附带时间戳，最后分数会以有效时间范围内的最后一次提交为准
○ 截止时间结束后，我们会人工复制有效的一次作业到一个专门的平台去查重，所以请大家尽量解决一下“评测系统发现的问题”那一栏提出的问题，降低我们的工作量


写在最后：我们本着让大家真正学到东西的心态，尽量努力将每一份作业判高分。各位被应试教育/ACM/OI坑惯了的同学请放轻松，这里不会出现特别离谱的edge case。也请同学们理解自动评测系统无法完美处理所有情况，所以请大家在有明确要求的点上按要求行事。

#启动llvm20
sudo docker start llvm20
sudo docker exec -it llvm20 bash

#进入工作区
cd /home/clr/work

# 复制整个项目目录到容器中
sudo docker cp /home/ubuntu/Desktop/ast-interpreter/. llvm20:/home/clr/work/

# 停止并删除旧容器
sudo docker stop llvm20 2>/dev/null
sudo docker rm llvm20 2>/dev/null

# 重新创建容器，挂载你的实际项目目录
sudo docker run -itd --name llvm20 -v /home/ubuntu/Desktop/ast-interpreter:/home/clr/work quay.io/bucloud/llvm-dev:20.1.8

#运行
./ast-interpreter "$(cat ../testcases/test00.c)"



clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test00.c)"
100
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test01.c)" 0
10
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test02.c)" 0
20
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test03.c)"
200
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test04.c)"
10
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test05.c)"
10
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test06.c)"
20
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test07.c)"
10
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test08.c)"
20
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test09.c)"
20
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test10.c)"
5
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test11.c)"
100
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test12.c)" error
4
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test13.c)" 0
20
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test14.c)" 0
12
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test15.c)" error
-8
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test16.c)" 0
30
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test17.c)"
10
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test18.c)" error 第二个0
10
20
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test19.c)" error
10
20
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test20.c)" error 0
Segmentation fault (core dumped)
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test22.c)"error 0 0
24
42
clr@c270710df100~/work/build $ ./ast-interpreter "$(cat ../testcases/test23.c)"
24
42
Aborted (core dumped)

