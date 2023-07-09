#include <iostream>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctime>
#include <string.h>
#include <fstream>
#include <vector>
#include <set>
#include "include/nlohmann/json.hpp"
#include <algorithm>
#include <linux/input-event-codes.h>

using json = nlohmann::json;
using namespace std;

#define MAX_LOG_FILE_SIZE 1000000 // 1MB

/*
    KNOWN BUGS:
    - Fix bug where when you press 2 buttons really fast, it only registers the first
        - Might just be a printing bug?
    --> Fixed:  Caused by the fact that sets were only compared based on timestamps, and the
                timestamp was the same for keys pressed simultaneously.
                Now, when the timestamp is the same it compares the keyInts instead.
                --> custom overload of operator< for the key struct
*/

/*
    REFRACTORING:
    - Debug bool should be held by the logger class
*/


/*
    --------------------
    | Logger Class     |
    --------------------
*/

class Logger
{
    public:
        int log(string message)
        {
            time_t now = time(0);
            string time = ctime(&now);
            int hours, minutes, seconds;
            sscanf(time.c_str(), "%*s %*s %*d %d:%d:%d", &hours, &minutes, &seconds);
            string timeString = to_string(hours) + ":" + to_string(minutes) + ":" + to_string(seconds) + " ";

            string log = "[*] " + timeString + message;
            cout << log.c_str() << endl;
            saveLogToFile(log);
            return 0;
        }
        int error(string message)
        {
            time_t now = time(0);
            string time = ctime(&now);
            int hours, minutes, seconds;
            sscanf(time.c_str(), "%*s %*s %*d %d:%d:%d", &hours, &minutes, &seconds);
            string timeString = to_string(hours) + ":" + to_string(minutes) + ":" + to_string(seconds) + " ";

            string log = "[x] " + timeString + message;
            cerr << log.c_str() << endl;
            saveLogToFile(log);
            return 0;
        }
        
        /* Dev */
        const bool showKeysHeld = false;
    private:
        const char* path = "log.txt";
        void saveLogToFile(string message);
};


void Logger::saveLogToFile(string message)
{
    ofstream file;
    file.open(path, ios::app); // Append mode
    file << message << endl;
    
    // If the file is too big, clear it
    if(file.tellp() > MAX_LOG_FILE_SIZE)
    {
        file.close();
        file.open(path, ios::out | ios::trunc); // Truncate mode, deletes contents of file before opening
        file << message << endl;
    }

    file.close();
}


/*
    --------------------
    | Keybinds Class   |
    --------------------
*/

class Keybinds
{
    private:
        struct key
        {
            int keyInt;
            int modifier;
            time_t timestamp;
            
            /* WHAT THE HELL O MY GOD OH HELL NO WHO INVITED THIS KID */
            /* 
                The error message you encountered, "no match for 'operator<' 
                (operand types are 'const Keybinds::key' and 'const Keybinds::key')", 
                suggests that there is no defined comparison operator (operator<) for the Keybinds::key struct. 
                This comparison operator is required when using std::set as a container for Keybinds::action objects, 
                as std::set uses this operator to order and compare its elements.

                To resolve the error, we need to provide an overloaded operator< for the Keybinds::key struct, 
                which allows comparisons between Keybinds::key objects.
            */
            bool operator<(const key& other) const
            {
                /* Ignore the modifier and keyInt when comparing keys, sort set based on unix timestamp */
                if(timestamp == other.timestamp)
                {
                    // If the timestamps are the same, sort based on keyInt. Prevent situations where you press simultaneously
                    return keyInt < other.keyInt; 
                }
                return timestamp < other.timestamp;
            }
        };
        struct action
        {
            set<key> keybind;
            string command;
        };
        /*
            - Cache consists of a vector of actions
            - Each action consists of a set of keybinds and a command
                - A set is used to prevent duplicate actions
                - A set is also used to easily find elements
            - Each keybind consists of a key and a modifier
            - For each event received, the keybinds are checked against the cache

            Example:
            [
                {
                    "keybind": [
                        {
                            "key": 28,
                            "modifier": 0
                        }
                    ],
                    "command": "echo Hello World"
                }
            ]
            This would execute the command "echo Hello World" when the key 28 is pressed

            - The user can dynamically change the keybinds by editing the disk file
            - The cache is reloaded when the disk file is changed
        */
        vector<action> cache;
        const char* file = "keybinds.json";

        /* 
            - Set containing the last DIFFERENT keys HELD 
                - A set is used to prevent duplicate actions
                - A set is also used to easily find elements
            - Used to detect keybinds
        */
        set<key> keysHeld;
        int updateKeysHeld(int _key, int modifier);

        Logger logger;

        void printCache();
    public:
        Keybinds()
        {
            reloadCache();
        };
        void updateDisk();
        void reloadCache();
        void checkKeybind(struct input_event ev);
};


int Keybinds::updateKeysHeld(int _key, int modifier)
{
    /*
        - Set should only contain keys that are currently held

        1. Check if key is already in set
        YES: 2.1 Check if key is unheld --> modifier of a key that is already in a set is 0
        NO: 2.2 Append the key to the set
    */
    
    key newKey;
    newKey.keyInt = _key;
    newKey.modifier = modifier;
    newKey.timestamp = time(0);
    
    /*
        Find_if function from algorithm
        Iterates over the set until it meets the lambda requirements, thus the keys being the same
        If the requirement was never met, the iterator will be equal to the end iterator
    */
    auto it = find_if(keysHeld.begin(), keysHeld.end(), [newKey](const key& existingKey) {
        return existingKey.keyInt == newKey.keyInt;
    });

    if(it == keysHeld.end())
    {
        // Key is not present in the set
        if(newKey.modifier != 0) /* Prevent buggy situations where the 0 modifier is the only key in the set */
        {
            keysHeld.insert(newKey);
        }
    }
    else
    {
        // Key is present in the set
        const key& foundKey = *it;

        if(foundKey.modifier != 0 && newKey.modifier == 0)
        {
            keysHeld.erase(it);
        }
    }

    if(logger.showKeysHeld)
    {
        cout << endl << "--------------------" << endl;
        for(const auto& element : keysHeld)
        {
            cout << "KEY: " << element.keyInt << " MODIFIER: " << element.modifier << " - ";
        }
        cout << endl << "--------------------" << endl;
    }
    
    return 0;
};


void Keybinds::printCache()
{
    cout << "Current keybindings: " << endl;
    cout << "--------------------" << endl;

    for(int i = 0; i < cache.size(); i++)
    {
        cout << "Action: " << cache[i].command << endl;
        cout << "Keybind: " << endl;
        for(const auto& element : cache[i].keybind)
        {
            cout << "KEY: " << element.keyInt << " - ";
        }
        cout << endl << "--------------------" << endl;
    }
};


void Keybinds::checkKeybind(struct input_event ev)
{
    updateKeysHeld(ev.code, ev.value);

    int matches;
    for (int i = 0; i < cache.size(); i++)
    {
        /* If the amount of keys held is not equal to the amount of keybind keys, continue to the next keybind */
        if (cache[i].keybind.size() != keysHeld.size())
        {
            continue;
        }
        matches = 0;
        for (auto it = cache[i].keybind.begin(); it != cache[i].keybind.end(); it++)
        {
            /* 
                Iterate through the keybinds of each action 
                Check if the held keys match the keybind of the action in any order
                - Make sure the keybind is not longer than the keys held
                If they do, execute the command
                If they don't, break out of the loop and continue to the next action
            */
            for (auto it2 = keysHeld.begin(); it2 != keysHeld.end(); it2++)
            {
                if (it->keyInt == it2->keyInt)
                {
                    matches++;
                    logger.showKeysHeld && logger.log("Matched key: " + to_string(it->keyInt) + " with key: " + to_string(it2->keyInt));
                }
            }
            if (matches == cache[i].keybind.size())
            {
                system(cache[i].command.c_str());
                logger.log("Executed command: " + cache[i].command);
                break;
            }
        }
    };
};


void Keybinds::reloadCache()
{
    cache.clear();

    ifstream inputFile(file);
    if(inputFile.is_open())
    {
        json data;
        inputFile >> data;

        for(const auto& actionJson : data)
        {
            action newAction;
            newAction.command = actionJson["command"];

            for(const auto& keybindJson : actionJson["keybind"])
            {
                key newKeybind;
                newKeybind.keyInt = keybindJson["key"];
                newKeybind.modifier = keybindJson["modifier"];
                
                newAction.keybind.insert(newKeybind);
            };
            
            cache.push_back(newAction);
        }

        inputFile.close();
        logger.log("Cache has been reloaded");
        printCache();
    }
    else
    {
        logger.error("Unable to open file: " + string(file));
    }
}


/*
    --------------------
    | Listener Class   |
    --------------------
*/

class Listener
{
    public:
        /* debug mode */
        bool debug = false;

        /* eventX to listen */
        const char* device;
        
        Logger logger;

        void init();
        void listen();
        void stop();
    private:
        /* Libevdev */
        struct libevdev *dev;
        int fd;
        int err;
        struct input_event ev;
        
        Keybinds keybinds;
};

void Listener::init()
{
    logger.log("Using device: " + string(device));
    fd = open(device, O_RDONLY); // Open device, blocking (wait for events)
    err = libevdev_new_from_fd(fd, &dev);
    if (err < 0) {
        logger.error("Failed to init libevdev on device: " + string(device));
        stop();
        exit(1);
    }
    debug && logger.log("Initialized libevdev on device: " + string(device));
}

void Listener::listen()
{
    debug && logger.log("Listening for events");
    while(true)
    {
        err = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        
        if (err == LIBEVDEV_READ_STATUS_SUCCESS) {
            
            if(ev.type == EV_KEY)
            {
                debug && logger.log("Event: type " + to_string(ev.type) + ", code " + to_string(ev.code) + ", value " + to_string(ev.value));
                keybinds.checkKeybind(ev);
            }
        
        } else {
            // Error occurred or the device was disconnected
            logger.error("Failed to get next event. Was the device disconnected?");
            stop();
            exit(1);
        }

        /* Prevent 100% CPU usage */
        usleep(1000);
    }
}

void Listener::stop()
{
    libevdev_free(dev);
    close(fd);
    debug && logger.log("Stopped libevdev");
}


/*
    --------------------
    | Main             |
    --------------------
*/

int main(int argc, char* argv[])
{
    Listener listener;
    string path;
    for(int i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "-d") == 0 && i + 1 < argc)
        {
            path = "/dev/input/" + string(argv[i + 1]);
            /* Check device validity */
            if(!ifstream(path.c_str()))
            {
                listener.logger.error("Device " + path + " does not exist");
                exit(1);
            }
            listener.device = path.c_str();
        }
        if(strcmp(argv[i], "--debug") == 0)
        {
            listener.debug = true;
        }
    }

    if(path.empty())
    {
        listener.logger.error("No device specified. Please specify a device with '-d eventX'");
        exit(1);
    }

    listener.init();
    listener.listen();

    listener.stop();
    return 0;
}