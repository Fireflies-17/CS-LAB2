# Shell实验

## 实验目的
更加熟悉进程控制和信号的概念。你将通过编写一个简单的Unix shell程序来实现这一目标，实现对作业的控制。

## 实验任务
查看tsh.c（微型shell）文件，你会看到它包含一个简单Unix shell的功能骨架。为了帮助你快速上手，我们已经实现了一些较不重要的函数。你的任务是完成下面列出的剩余空函数。为了帮助你进行合理检查，我们已在我们的参考解决方案中列出了每个函数的代码行数的近似值（其中包含大量注释）。

* eval：解析和解释命令行的主例程。【70行】
* builtin cmd：识别和解释内置命令：quit、fg、bg和jobs。【25行】
* do_bgfg：实现bg和fg内置命令。【50行】
* waitfg：等待前台作业完成。【20行】
* sigchld handler：捕获SIGCHILD信号。【80行】
* sigint handler：捕获SIGINT（ctrl-c）信号。【15行】
* sigtstp handler：捕获SIGTSTP（ctrl-z）信号。【15行】
  
每次修改tsh.c文件后，输入make以重新编译它。要运行您的shell，请在命令行中输入tsh：

```
linux> ./tsh
tsh> [命令]
```

## shell介绍

Shell是一个交互式命令行解释器，代表用户运行程序。Shell重复地打印提示符，等待stdin上输入的命令行，然后根据命令行的内容执行一些操作。

命令行是一系列由空白分隔的ASCII文本单词。命令行中的第一个单词要么是内置命令的名称，要么是可执行文件的路径名。其余的单词是命令行参数。如果第一个单词是内置命令，Shell立即在当前进程中执行该命令。否则，该单词被假定为可执行程序的路径名。在这种情况下，Shell创建一个子进程，然后在子进程的上下文中加载并运行程序。作为解释单个命令行的结果创建的子进程总称为作业。一般来说，一个作业可以由通过Unix管道连接的多个子进程组成。

如果命令行以一个 "&" 结尾，那么作业在后台运行，这意味着Shell在打印提示并等待下一个命令行之前不等待作业终止。否则，作业在前台运行，这意味着Shell等待作业终止后才等待下一个命令行。因此，在任何时候，最多只能有一个作业在前台运行。但是，任意数量的作业可以在后台运行。

例如，输入以下命令行：

```
tsh> jobs
```

shell将执行内置的jobs命令。输入以下命令行：

```
/bin/ls -l -d
```

在前台运行ls程序。shell应确保当程序开始执行其主函数时，argc和argv参数具有以下值：
```
int main(int argc, char *argv[])
```

* argc == 3
* argv[0] == "/bin/ls"
* argv[1] == "-l"
* argv[2] == "-d"

输入以下命令行，将在后台运行ls程序
```
tsh> /bin/ls -l -d &
```

Shell 支持作业控制的概念，允许用户在后台和前台之间移动作业，并更改作业中进程的状态（运行、停止或终止）。输入ctrl-c会导致一个SIGINT信号被发送到前台作业中的每个进程。SIGINT的默认操作是终止进程。类似地，输入ctrl-z会导致一个SIGTSTP信号被发送到前台作业中的每个进程。SIGTSTP的默认操作是将进程置于停止状态，直到接收到SIGCONT信号唤醒它。Shells还提供各种内置命令来支持作业控制。例如：

* jobs：列出正在运行和停止的后台作业。
* bg <job>：将一个停止的后台作业更改为正在后台运行的作业。
* fg <job>：将一个停止或正在后台运行的作业更改为前台运行。
* kill <job>：终止一个作业。

### tsh实现规范

tsh规范应包括以下特性：

* 提示符应为字符串"tsh>"。

* 用户键入的命令行应包括一个名称和零个或多个参数，所有参数之间用一个或多个空格分隔。如果名称是一个内置命令，那么tsh应立即处理它并等待下一个命令行。否则，tsh应假定名称是可执行文件的路径，它将在初始子进程的上下文中加载并运行（在这个上下文中，术语“作业”指的是这个初始子进程）。
  
* tsh无需支持管道（|）或I/O重定向（<和>）。
  
* 输入ctrl-c（ctrl-z）应导致将SIGINT（SIGTSTP）信号发送到当前前台作业，以及该作业的任何子进程（例如：它fork的任何子进程）。如果没有前台作业，则该信号不应产生任何效果。

* 如果命令行以一个 & 结尾，那么tsh应在后台运行作业。否则，它应在前台运行作业。

* 每个作业可以由进程ID（PID）或作业ID（JID）标识，JID是由tsh分配的正整数。JID应在命令行上用前缀'%'表示。例如，"%5"表示JID 5，"5"表示PID 5。（我们已经提供了操作作业列表所需的所有例程。）
  
* tsh应支持以下内置命令：

  - quit命令终止shell。
  - jobs命令列出所有后台作业。
  - bg <job>命令通过发送SIGCONT信号重新启动<job>，然后在后台运行它。 <job>参数可以是PID或JID。
  - fg <job>命令通过发送SIGCONT信号重新启动<job>，然后在前台运行它。 <job>参数可以是PID或JID。
  
* tsh应收集所有僵尸子进程。如果任何作业因为接收到它没有捕获的信号而终止，那么tsh应识别此事件并打印一个带有作业的PID和有关号的描述的消息。

## 检查你的工作

我们提供了一些工具来帮助检查你的工作。

参考实现：Linux可执行文件tshref是shell的参考实现。运行此程序以解决你对shell应该如何工作的任何疑问。您的shell应该产生与参考实现拥有相同的输出（当然，除了PID，因为它们在每次运行时都会更改）。

shell driver：sdriver.pl程序将shell作为子进程执行，按照跟踪文件的指示发送命令和信号，并捕获和显示shell的输出。
使用-h参数查看sdriver.pl的用法：
```
linux> ./sdriver.pl -h
Usage: sdriver.pl [-hv] -t <trace> -s <shellprog> -a <args>
Options:
  -h Print this message
  -v Be more verbose
  -t <trace> Trace file
  -s <shell> Shell program to test
  -a <args> Shell arguments
  -g Generate output for autograder
```

我们还提供了16个跟踪文件（trace{01-16}.txt），这些将与shell驱动程序一起使用，以测试你所实现的shell的正确性。较低编号的跟踪文件执行非常简单的测试，而较高编号的测试执行更复杂的测试。

你可以通过键入以下命令，在你的shell上使用trace文件trace01.txt（例如）运行shell驱动程序：

```
linux> ./sdriver.pl -t trace01.txt -s ./tsh -a "-p"
```

（-a "-p"参数告诉您的shell不要发出提示符）,或者执行

```
linux> make test01
```

同样，要将你的结果与参考shell进行比较，您可以在参考shell上运行跟踪驱动程序，方法是输入：
```
linux> ./sdriver.pl -t trace01.txt -s ./tshref -a "-p"
```

或

```
linux> make rtest01
```

tshref.out提供了参考实现在所有跟踪上的输出。这对你而言可能比手动在所有跟踪文件上运行shell驱动程序更方便。

跟踪文件的好处是它们生成和你以交互方式运行shell时相同的输出（除了一个标识跟踪的初始注释）。例如：

```
linux> make test15
./sdriver.pl -t trace15.txt -s ./tsh -a "-p"
#
# trace15.txt - Putting it all together
#
tsh> ./bogus
./bogus: Command not found.
tsh> ./myspin 10
Job (9721) terminated by signal 2
tsh> ./myspin 3 &
[1] (9723) ./myspin 3 &
tsh> ./myspin 4 &
[2] (9725) ./myspin 4 &
tsh> jobs
[1] (9723) Running ./myspin 3 &
[2] (9725) Running ./myspin 4 &
tsh> fg %1
Job [1] (9723) stopped by signal 20
tsh> jobs
[1] (9723) Stopped ./myspin 3 &
[2] (9725) Running ./myspin 4 &
tsh> bg %3
%3: No such job
tsh> bg %1
[1] (9723) ./myspin 3 &
tsh> jobs
[1] (9723) Running ./myspin 3 &
[2] (9725) Running ./myspin 4 &
tsh> fg %1
tsh> q
```

## 提示

* 仔细阅读教材第8章（异常控制流）。
 
* 使用跟踪文件指导shell开发。从trace01.txt开始，确保你的shell产生与参考shell相同的输出。然后继续进行trace02.txt等跟踪文件。

* waitpid、kill、fork、execve、setpgid和sigprocmask函数将非常有用。waitpid的WUNTRACED和WNOHANG选项也会很有用

* 在实现信号处理程序时，确保使用kill函数的参数中的"-pid"而不是"pid"向整个前台进程组发送SIGINT和SIGTSTP信号。sdriver.pl程序会检测此错误。
  
* 任务分配的一个棘手部分是决定waitfg和sigchld_handler函数之间的工作分配。我们建议采用以下方法：
  - 在waitfg中，使用围绕sleep函数的忙循环。
  - 在sigchld处理程序中，使用一次waitpid调用。
  
尽管其他方案也是可行的，比如在waitfg和sigchld处理程序中都调用waitpid，但这样做代码不易理解。在处理程序中完成所有清理工作更为简单。

* 在eval中，父进程在fork子进程之前必须使用sigprocmask阻塞SIGCHLD信号，然后在通过调用addjob将子进程添加到作业列表后，再次使用sigprocmask解除对这些信号的阻塞。由于子进程继承了其父进程的阻塞向量，子进程必须确保在执行新程序之前解除对SIGCHLD信号的阻塞。父进程需要以这种方式阻塞SIGCHLD信号，以避免在父进程调用addjob之前，子进程被sigchld_handler收回（因此从作业列表中删除）的竞争条件。
  
* 像more、less、vi和emacs这样的程序对终端设置进行了奇怪的操作。不要从你的shell中运行这些程序。只使用简单的基于文本交互的程序，如/bin/ls、/bin/ps和/bin/echo。

* 当你从标准Linux shell运行您的shell时，您的shell正在前台进程组中运行。如果您的shell然后创建一个子进程，那么默认情况下，该子进程也将成为前台进程组的成员。由于键入ctrl-c会向前台组中的每个进程发送一个SIGINT，键入ctrl-c将向您的shell以及您的shell创建的每个进程发送一个SIGINT，这显然是不正确的。
以下是解决方法：在fork之后但在execve之前，子进程应调用setpgid(0, 0)，将子进程放入一个新的进程组，其组ID与子进程的PID相同。这确保在前台进程组中只有一个进程，即您的shell。当您键入ctrl-c时，shell应该捕获产生的SIGINT，然后将其转发给相应的前台作业（更准确地说，包含前台作业的进程组）。

## 评价方法
分数将根据以下比例计算，最大分数为90分：

* 80分正确性：每个5分的16个跟踪文件。

* 10分代码风格分：代码有良好的注释（5分），并检查每个系统调用的返回值（5分）。
  
我们将在Linux机器上使用相同的shell驱动程序和跟踪文件对你的解决方案进行正确性测试，这些文件已包含在您的实验目录中。你的shell在这些跟踪上应该产生与shell的参考实现有相同的输出，只有两个例外：

* PIDs可能会不同。

* trace11.txt、trace12.txt和trace13.txt中/bin/ps命令的输出在每次运行时都会有所不同。然而，在/bin/ps命令输出中，任何mysplit进程的运行状态应该是相同的。

## 实验代码包内容说明

文件：

Makefile        # 编译脚本

README.md       # 本文件

tsh.c           # shell程序文件

tshref          # tsh的参考实现程序

# 用于测试的文件
sdriver.pl      # shell测试程序

trace*.txt      # 15个测试文件，由sdriver.pl使用

tshref.out      # 15个测试文件的示例输出

# 测试的示例程序，不需要修改
myspin.c        # Takes argument <n> and spins for <n> seconds

mysplit.c       # Forks a child that spins for <n> seconds

mystop.c        # Spins for <n> seconds and sends SIGTSTP to itself

myint.c         # Spins for <n> seconds and sends SIGINT to itself

