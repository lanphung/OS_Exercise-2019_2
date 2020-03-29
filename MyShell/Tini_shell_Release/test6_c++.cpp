#include <cstdlib>    //<stdlib.h> for c++, malloc(), realloc(), free(), exit(), execvp(), EXIT_SUCCESS, EXIT_FAILURE
#include <unistd.h>   //for chdir(), fork(), exec(), pid_t
#include <cstdio>     //<stdio.h> for c++, for fprintf(), printf(), stderr, getchar() etc.
#include <sys/wait.h> //for waitpid() and its macros
#include <string>     //for string operations
#include <iostream>
#include <vector>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <ctime>
//#include <regex>

// test :  thiếu lệnh ps, start chương trình =  mode bg và fg, stop command
// idea : coppy all running process in system to jobs in this shell to manage it

using namespace std;

struct job
{
    int pid;
    int running;
    string cmd;
};

pid_t fgProcess = 0;
int toBg = 0;
int pipe_ints[2];

void sigint_handler(int s)
{
    //cout << "Signal received." << endl;
    if (fgProcess != 0)
        kill(fgProcess, SIGKILL);
    fgProcess = 0;
    return;
}

void sigtstp_handler(int s)
{
    if (fgProcess != 0)
        kill(fgProcess, SIGSTOP);
    toBg = 1;
    return;
}

vector<job> jobs;

vector<string> history;

vector<string> split(string command, char delim)
{
    int i;
    vector<string> tokens;
    string token = "";
    for (i = 0; i < command.length(); i++)
    {
        if (command[i] != delim)
            token += command[i];
        else
        {
            while (command[i + 1] == delim && i < command.length())
                i++;
            if (token.length() > 0)
            {
                tokens.push_back(token);
                token.clear();
            }
        }
    }
    if (token.length() > 0)
        tokens.push_back(token);
    return tokens;
}

void cdCmd(vector<string> tokens)
{
    if (tokens.size() == 1)
    {
        cout << "Need one more parameter." << endl;
        return;
    }
    else if (tokens.size() > 2)
    {
        cout << "Invalid Command." << endl;
        return;
    }
    else
    {
        if (chdir(tokens[1].c_str()) != 0)
            cout << "Invalid address." << endl;
        history.push_back("cd " + tokens[1]);
        return;
    }
}

void helpCmd(vector<string> tokens)
{
    if (tokens.size() > 1)
    {
        cout << "help command doesn't take any arguments." << endl;
        return;
    }
    cout << "List of built-in commands:\ncd, help, history, exit, jobs, kill, bg and fg" << endl;
    cout << "Usage:\n 1. cd: cd <path to directory(absolute/relative)>(changes current directory to the one specified)" << endl;
    cout << " 2. help: help(shows the shell's manual)" << endl;
    cout << " 3. history: history(shows the list of previously used commands)\n   !n command can be used to run the nth event stored in history(n can be positve or negative)" << endl;
    cout << " 4. exit: exit(terminates the shell, there shouldn't be any background processes for the shell to terminate)" << endl;
    cout << " 5. jobs: jobs(shows the list of background processes)" << endl;
    cout << " 6. kill: kill %n/ kill pid\n   n is the job number assigned to a background process that can be viewed using the command jobs, pid is the process id of the particular process you want to kill" << endl;
    cout << " 7. Ctrl+C can be used to kill a foreground process running on shell" << endl;
    cout << " 8. Ctrl+Z can be used to send a foreground process to background and stops it" << endl;
    cout << " 9. bg %n can be used to start a stopped process in the background, where n is the job number" << endl;
    cout << "10. fg %n can be used to bring a background process to foreground, where n is the job number" << endl;
    cout << "11. command_1 | command_2 can be used to pipe the output of first process as the input of the second one." << endl;
    history.push_back("help");
    return;
}

void exitCmd(vector<string> tokens)
{
    if (tokens.size() > 1)
    {
        cout << "exit command doesn't take any arguments." << endl;
        return;
    }
    history.push_back("exit");
    if (jobs.size() == 0)
    {
        cout << "Goodbye !!!";
        exit(EXIT_SUCCESS);
    }
    else
    {
        cout << "Some background processes exist. You need to kill the background processes to terminate." << endl;
        cout << "Are you sure ? (Y/N) ? ";
        string acceptCommand;
        getline(cin, acceptCommand);
        if (acceptCommand == "Y" || acceptCommand == "y")
        {
            for (int i = 0; i < jobs.size(); i++)
            {
                kill(jobs[i].pid, SIGKILL);
            }
            exit(EXIT_SUCCESS);
        }
        else if (acceptCommand == "N" || acceptCommand == "n")
        {
            //
        }
    }
    return;
}

void historyCmd(vector<string> tokens)
{
    history.push_back("history");
    for (int i = 0; i <= history.size() - 1; i++)
        cout << i + 1 << ". " << history[i] << endl;
    return;
}

void timeCmd(vector<string> tokens)
{
    if (tokens.size() > 1)
    {
        cout << "time command doesn't take any arguments." << endl;
        return;
    }
    // current date/time based on current system
    time_t now = time(0);
    // convert now to string form
    char *dt = ctime(&now);
    cout << "The local date and time is: " << dt;
    // convert now to tm struct for UTC
    tm *gmtm = gmtime(&now);
    dt = asctime(gmtm);
    cout << "The UTC   date and time is: " << dt;

    history.push_back("time");
    return;
}

void pathCmd(vector<string> tokens)
{
    if (tokens.size() > 2)
    {
        cout << "Invalid arguments." << endl;
        return;
    }

    if (tokens.size() == 1)
    {
        history.push_back("path");
        system("printenv");
        return;
    }

    history.push_back("path " + tokens[1]);
    string commandGetPath = "printenv ";
    commandGetPath.append(tokens[1]);
    const char *temp = commandGetPath.c_str();
    // system(temp);

    char *result = getenv(tokens[1].c_str());
    if (result != NULL)
    {
        cout << result << endl;
    }
    else
    {
        cout << "Dont  have this path in environment variables" << endl;
    }
}

void addPathCmd(vector<string> tokens)
{
    if (tokens.size() != 2)
    {
        cout << "Invalid arguments." << endl;
        return;
    }

    history.push_back("addpath " + tokens[1]);

    char *string_var = new char[tokens[1].length()];
    strcpy(string_var, tokens[1].c_str());
    putenv(string_var); //each shell has other environment variables
}

void jobsCmd(vector<string> tokens)
{
    history.push_back("jobs");
    if (jobs.size() == 0)
    {
        cout << "There are no current jobs." << endl;
        return;
    }

    cout << "Num\tPID\tStatus\tCMD\n";
    for (int i = 0; i < jobs.size(); i++)
        if (jobs[i].running)
        {
            cout << '[' << i + 1 << "]\t" << jobs[i].pid << "\tRunning\t" << jobs[i].cmd << endl;
        }
        else
        {
            cout << '[' << i + 1 << "]\t" << jobs[i].pid << "\tStopped\t" << jobs[i].cmd << endl;
        }
}

void bgCmd(vector<string> tokens)
{
    history.push_back("bg " + tokens[1]);
    if (tokens[1][0] == '%')
    {
        int number = atoi(tokens[1].substr(1).c_str());
        if (number > jobs.size())
        {
            cout << "Invalid Job number." << endl;
            return;
        }
        kill(jobs[number - 1].pid, SIGCONT);
        if (!jobs[number - 1].running)
        {
            jobs[number - 1].running = 1;
            cout << "Job number " << number << " has been started in the background." << endl;
        }
        else
            cout << "Job number " << number << " is already running." << endl;
    }
}

void fgCmd(vector<string> tokens)
{
    history.push_back("fg " + tokens[1]);
    if (tokens[1][0] == '%')
    {
        int number = atoi(tokens[1].substr(1).c_str());
        int status;
        if (number > jobs.size())
        {
            cout << "Invalid Job number." << endl;
            return;
        }
        pid_t pid = jobs[number - 1].pid, w_pid;
        string command = jobs[number - 1].cmd;
        vector<job>::iterator i;
        if (!jobs[number - 1].running)
            kill(pid, SIGCONT);
        for (i = jobs.begin(); i != jobs.end(); i++)
        {
            if (i->pid == pid)
            {
                jobs.erase(i);
                break;
            }
        }
        do
        {
            w_pid = waitpid(pid, &status, WNOHANG);
            signal(SIGINT, sigint_handler);
            signal(SIGTSTP, sigtstp_handler);
            if (toBg)
            {
                job instance;
                instance.pid = pid;
                instance.running = 0;
                instance.cmd = command;
                jobs.push_back(instance);
                toBg = 0;
                fgProcess = 0;
                cout << "Job added to the background and stopped." << endl;
                break;
            }
        } while (w_pid == 0);
    }
}

void killCmd(vector<string> tokens)
{
    history.push_back("kill " + tokens[1]);
    if (tokens[1][0] == '%')
    {
        int number = atoi(tokens[1].substr(1).c_str());
        if (number > jobs.size())
        {
            cout << "Invalid Job number." << endl;
            return;
        }
        kill(jobs[number - 1].pid, SIGKILL);
        vector<job>::iterator i;
        int j = 0;
        for (i = jobs.begin(); i != jobs.end(); i++)
        {
            j++;
            if (j == number)
            {
                jobs.erase(i);
                cout << "Job number " << number << " has been terminated." << endl;
                return;
            }
        }
    }
    else
    {
        int pid = atoi(tokens[1].c_str());
        kill(pid, SIGKILL);
        vector<job>::iterator i;
        for (i = jobs.begin(); i != jobs.end(); i++)
        {
            if (i->pid == pid)
            {
                jobs.erase(i);
                cout << "Process with PID " << pid << " has been terminated." << endl;
                return;
            }
        }
    }
}

void launch(vector<string> tokens, int pipeFlag)
{

    pid_t pid, w_pid;
    FILE *inFile;
    FILE *outFile;
    string command = "";
    int gFlag = 0, lFlag = 0; // we dont need this
    for (int i = 0; i < tokens.size(); i++)
    {
        if (tokens[i] == "<")
            lFlag = i;
        else if (tokens[i] == ">" || tokens[i] == ">>")
            gFlag = i;
        command.append(tokens[i]);
        command.append(" ");
        //else if (regex_match(tokens[i], regex("!(\\+|-)?[[:digit:]]+"))) {
    }
    if (pipeFlag == -1 || pipeFlag == 0)
        history.push_back(command);
    if (pipeFlag == 1)
        history[history.size() - 1].append("| " + command);
    if (lFlag)
    {
        inFile = fopen(tokens[lFlag + 1].c_str(), "r");
        if (!inFile)
        {
            cout << "Invalid Input File." << endl;
            return;
        }
        //dup2(fileno(inFile), 0);
    }
    if (gFlag)
    {
        if (tokens[gFlag] == ">")
            outFile = fopen(tokens[gFlag + 1].c_str(), "w");
        else if (tokens[gFlag] == ">>")
            outFile = fopen(tokens[gFlag + 1].c_str(), "a");
        //dup2(fileno(outFile), 1);
    }
    if (lFlag || gFlag)
    {
        if (lFlag)
        {
            while (tokens.size() > lFlag)
                tokens.pop_back();
        }
        else
        {
            while (tokens.size() > gFlag)
                tokens.pop_back();
        }
    }
    int flag = 0;
    string last_param = tokens[tokens.size() - 1];
    int last_param_len = last_param.length() - 1;
    if (last_param[last_param_len] == '&')
    {
        tokens[tokens.size() - 1] = last_param.substr(0, last_param_len);
        flag = 1;
    }
    // Important: Vector conversion to array of strings
    const char **argv = new const char *[tokens.size() + 1];
    for (int j = 0; j < tokens.size(); j++)
        argv[j] = tokens[j].c_str();
    argv[tokens.size()] = NULL;
    int status;
    pid = fork();
    if (pid == 0)
    {
        if (lFlag)
            dup2(fileno(inFile), 0);
        if (gFlag)
            dup2(fileno(outFile), 1);
        if (pipeFlag == 0)
        {
            close(1);
            close(pipe_ints[0]);
            dup2(pipe_ints[1], 1);
        }
        else if (pipeFlag == 1)
        {
            close(0);
            close(pipe_ints[1]);
            dup2(pipe_ints[0], 0);
        }
        execvp(argv[0], (char **)argv);
        cout << "Invalid Command." << endl;
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("Shell Error: Error forking process.");
    }
    else
    {
        //do {
        fgProcess = pid;
        if (!flag)
            do
            {
                w_pid = waitpid(pid, &status, WNOHANG);
                signal(SIGINT, sigint_handler);
                signal(SIGTSTP, sigtstp_handler);
                if (toBg)
                {
                    job instance;
                    instance.pid = pid;
                    instance.running = 0;
                    instance.cmd = command;
                    jobs.push_back(instance);
                    toBg = 0;
                    fgProcess = 0;
                    cout << "Job added to the background and stopped." << endl;
                    break;
                }
            } while (w_pid == 0);
        else
        {
            w_pid = waitpid(pid, &status, WNOHANG);
            if (w_pid == 0)
            {
                job instance;
                instance.pid = pid;
                instance.running = 1;
                instance.cmd = command.substr(0, command.length() - 2);
                jobs.push_back(instance);
            }
        }
        //fgProcess = pid;
        //} while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    if (lFlag)
        fclose(inFile);
    if (gFlag)
        fclose(outFile);
    return;
}

void execute(vector<string> tokens, int pipeFlag)
{
    if (tokens[0] == "cd")
    {
        cdCmd(tokens);
    }
    else if (tokens[0] == "help")
    {
        helpCmd(tokens);
    }
    else if (tokens[0] == "exit")
    {
        exitCmd(tokens);
    }
    else if (tokens[0] == "history")
    {
        historyCmd(tokens);
    }
    else if (tokens[0] == "time" || tokens[0] == "date")
    {
        timeCmd(tokens);
    }
    else if (tokens[0] == "path")
    {
        pathCmd(tokens);
    }
    else if (tokens[0] == "addpath")
    {
        addPathCmd(tokens);
    }
    else if (tokens[0] == "jobs")
    {
        jobsCmd(tokens);
    }
    else if (tokens[0] == "kill")
    {
        killCmd(tokens);
    }
    else if (tokens[0] == "bg")
    {
        bgCmd(tokens);
    }
    else if (tokens[0] == "fg")
    {
        fgCmd(tokens);
    }
    else if (tokens[0] == "list") //is "ps -a" command in linux
    {
        vector<string> newTokens;
        newTokens.push_back("ps");
        newTokens.push_back("-a");

        launch(newTokens, pipeFlag);
    }
    else if (tokens[0] == "stop") //Ctrl+Z
    {
        fgCmd(tokens);
    }
    else if (tokens[0] == "resume") //use command "bg" with this job stopped
    {
        tokens[0] = "bg";
        bgCmd(tokens);
    }
    else
        launch(tokens, pipeFlag); // lauch external command
    return;
}

void shellLoop()
{
    signal(SIGINT, sigint_handler);
    string path = get_current_dir_name();
    vector<string> tokens;
    // path.append(" > ");
    string command;
    int status;
    while (true)
    {
        cout << "\nDir: " << path << endl
             << ">>> ";
        getline(cin, command);
        pid_t pid = waitpid(pid, &status, WNOHANG);
        pipe(pipe_ints);
        tokens = split(command, '|');
        if (tokens.size() > 1)
        {
            vector<string> tokensCmd1 = split(tokens[0], ' ');
            execute(tokensCmd1, 0);
            vector<string> tokensCmd2 = split(tokens[1], ' ');
            execute(tokensCmd2, 1);
            //history.push_back(command);
            tokensCmd1.clear();
            tokensCmd2.clear();
        }
        else
        {
            tokens = split(tokens[0], ' ');

            for (int i = 0; i < tokens.size(); i++)
            {
                cout << "tokens[" << i << "] : " << tokens[i] << endl;
            }

            execute(tokens, -1);
        }
        if (pid > 0)
        {
            //cout << "Process with PID " << pid << " has been terminated." << endl;
            int j = 0;
            vector<job>::iterator i;
            for (i = jobs.begin(); i != jobs.end(); i++)
            {
                j++;
                if (i->pid == pid)
                {
                    cout << '[' << j << ']' << " " << pid << " Done " << i->cmd << endl;
                    jobs.erase(i);
                    break;
                }
            }
        }
        command.clear();
        path.clear();
        tokens.clear();
        path = get_current_dir_name();
        // path.append(">");
    }
    return;
}

int main(int argc, char **argv)
{
    shellLoop();
    return 0;
}
