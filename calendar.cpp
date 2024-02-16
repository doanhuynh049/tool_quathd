#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <climits>
struct Task
{
    std::string date;
    std::string starttime;
    std::string endtime;
    std::string description;
    std::string status;
};
struct TargetTask
{
    std::string description;
    std::chrono::system_clock::time_point deadline;
    int progress;
};
void notifyTask(const Task &task)
{
#ifdef _WIN32
#elif defined(__linux__)
    std::cout << "\033[1;34m"; // Blue text
    std::cout << "It's time to " << task.description << " on " << task.date << " at " << task.starttime << " ! " << std::endl;
    std::cout << "\033[0m"; // Reset color
#else
#error Unsupported platform
#endif
}
std::vector<Task> loadTasksFromCSV(const std::string &filename)
{
    std::vector<Task> tasks;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return tasks;
    }
    std::string line;
    std::getline(file, line); // skip header row
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        Task task;
        std::getline(ss, task.date, ',');
        std::getline(ss, task.starttime, ',');
        std::getline(ss, task.endtime, ',');
        std::getline(ss, task.description, ',');
        std::getline(ss, task.status, ',');
        // std::cout << "task date: " << task.date << " task time: " << task.starttime
        //           << " end time: " << task.endtime << " task description " << task.description
        //           << " task status: " << task.status << " %" << std::endl;
        tasks.push_back(task);
    }

    file.close();
    return tasks;
}
std::vector<TargetTask> loadTargetFromCSV(const std::string &filename)
{
    std::vector<TargetTask> targetTask;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return targetTask;
    }
    std::string line;
    std::getline(file, line); // skip header row
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        Task task;
        TargetTask targettask;
        std::string typeoftask;
        std::getline(ss, typeoftask, ',');
        if (typeoftask == "TARGET")
        {
            targettask.progress = 0;
            std::string deadline;
            std::getline(ss, deadline, ',');
            std::getline(ss, targettask.description, ',');
            std::tm targetTM = {};
            std::istringstream targetDateStream(deadline);
            targetDateStream >> std::get_time(&targetTM, "%Y-%m-%d");
            targettask.deadline = std::chrono::system_clock::from_time_t(std::mktime(&targetTM));
            std::cout << "Description: " << targettask.description << " deadline: ";
            std::cout << " progress: " << targettask.progress << std::endl;
            targetTask.push_back(targettask);
        }
        else
        {
        }
        file.close();
        return targetTask;
    }
}
void waitUntilEndTime(const Task &task)
{
    std::tm tm = {};
    std::istringstream endStream(task.date + " " + task.endtime);
    endStream >> std::get_time(&tm, "%Y-%m-%d %H:%M");
    std::chrono::system_clock::time_point endTime = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    std::this_thread::sleep_until(endTime);
}
void updateTaskStatusInCSV(const Task &task, const std::string &status)
{
    std::fstream file("tasks_date.csv", std::ios::in | std::ios::out);
    if (!file.is_open())
    {
        std::cerr << "Error opening file: tasks_date.csv" << std::endl;
        return;
    }
    std::string line;
    std::string updatedLine;
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string taskDate, taskStartTime, taskEndTime, taskDescription, taskStatus;
        std::getline(ss, taskDate, ',');
        std::getline(ss, taskStartTime, ',');
        std::getline(ss, taskEndTime, ',');
        std::getline(ss, taskDescription, ',');

        if (taskDate == task.date && taskStartTime == task.starttime && taskEndTime == task.endtime)
        {
            updatedLine = task.date + "," + task.starttime + "," + task.endtime + "," + task.description + "," + status;

            // Explicitly cast to std::streamoff to avoid ambiguity
            file.seekp(static_cast<std::streamoff>(file.tellg()) - static_cast<std::streamoff>(line.length()) - 1);

            file << std::left << std::setw(line.length()) << updatedLine; // Write the updated line
            break;                                                        // Break out of the loop since the line is updated
        }
    }
    file.close();
}
void performTask(Task &task){
    notifyTask(task);
    waitUntilEndTime(task);
    std::cout << "\033[1;32m";
    std::cout << "Task has ended: " << task.description << std::endl;
    std::cout << "\033[0m";
    do
    {
        std::cout << "Enter task status (0 to 100%): ";
        std::cin >> task.status;
        if (std::cin.fail() || std::stoi(task.status) < 0 || std::stoi(task.status) > 100)
        {
            std::cin.clear();
            std::cin.ignore(INT_MAX, '\n');
            std::cerr << "Invalied input. Please enter a valied percentage (0 to 100)" << std::endl;
        }
        else
        {
            break;
        }
    } while (true);
    updateTaskStatusInCSV(task, task.status);
}
void processTasks(std::vector<Task> &tasks){
    for (size_t i = 0; i<tasks.size();i++)
    {
        Task& currentTask = tasks[i];
        std::time_t currentTime = std::time(nullptr);

        std::tm tm1 = {};
        std::tm tm2 = {};

        std::istringstream startStream(currentTask.date + " " + currentTask.starttime);
        std::istringstream endStream(currentTask.date + " " + currentTask.endtime);

        startStream >> std::get_time(&tm1, "%Y-%m-%d %H:%M");
        endStream >> std::get_time(&tm2, "%Y-%m-%d %H:%M");

        std::time_t startTime = std::mktime(&tm1);
        std::time_t endTime = std::mktime(&tm2);
        // std::cout << "startime " << startTime << "end time: " << endTime << "currne time: " << currentTime << std::endl;
        if (startTime <= currentTime && currentTime <= endTime )
        {
            // std::cout << "currentTask date: " << currentTask.date << " currentTask time: " << currentTask.starttime << " currentTask description " << currentTask.description << std::endl;
            std::cout << "Task is currently ongoing: " << currentTask.description << std::endl;
            // std::chrono::system_clock::time_point taskTimePoint = std::chrono::system_clock::from_time_t(taskTime);
            // std::this_thread::sleep_until(taskTimePoint);
            std::thread taskThread(performTask, std::ref(tasks[i]));
            taskThread.join();
            
        }
        // Check for the next task and schedult it 
        if (i+1 < tasks.size()){
            const Task &nextTask = tasks[i+1];
            std::tm tm2={};
            std::istringstream nextStartStream(nextTask.date +" "+nextTask.starttime);
            nextStartStream >> std::get_time(&tm2,"%Y-%m-%d %H:%M");
            std::chrono::system_clock::time_point nextTaskStartTime = std::chrono::system_clock::from_time_t(std::mktime(&tm2));
            std::chrono::duration<double> timeUntilNextTask = nextTaskStartTime - std::chrono::system_clock::now();
            if(timeUntilNextTask.count() > 0){
                std::this_thread::sleep_for(timeUntilNextTask);
            }
        }
    }
}
int main()
{
    const std::string csvFilePath = "/home/quathd/tool_quathd/tasks_date.csv";
    std::vector<Task> tasks = loadTasksFromCSV(csvFilePath);
    processTasks(tasks);
    return 0;
}