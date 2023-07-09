#include <iostream>
#include <mpd/client.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/keyboard.h>
#include <linux/input.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/uinput.h>
#include <string>
using namespace std;

/*
TODO:
- Input flags for mode selection
- Support for holding CTRL and pressing arrow keys instead of having to let go of the CTRL key each time
- A way to search for keyboard devices instead of hardcoding the path
- Configure atExit function to clean up memory
*/


class Player
{
    private:
        // 0 = MPD mode, 1 = Simulated keyboard mode
        unsigned short int mode;

        struct mpd_connection *conn;
        struct libevdev* evdev;
        
        char device[20] = "/dev/input/event25"; // Keyboard device
        int fd; // Keyboard device
        // Keyboard event outside of class
        
        char sDevice[20] = "/dev/input/event25"; // Simulated keyboard device
        int uinput_fd; // Virtual keyboard device for mode 0
        struct input_event sEv; // Simulated keyboard event
    public:
        Player();
        void next();
        void previous();
        void play();
        void volumeUp();
        void volumeDown();
        int getEvent(input_event &ev);
        void free(); // Free memory at exit
        void writeSimulatedKey(unsigned short int key);
        // void arguments(int argc, char* argv[]);
};

Player::Player()
{
    /*
    Creates a new connection on port 6000
    30s timeout
    On localhost (IP NULL)
    */
    conn = mpd_connection_new(NULL, 0, 30000);
    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        cout << "Error: " << mpd_connection_get_error_message(conn) << endl;
        cout << "MPD Might not be running." << endl;
        mpd_connection_free(conn);
        cout << "Using default mode (simulated media buttons)" << endl;
        mode = 1;

        // Create virtual keyboard device
        uinput_fd = open(sDevice, O_WRONLY | O_NONBLOCK);
        if (uinput_fd < 0) {
            cerr << "Failed to open " << sDevice << endl;
            close(uinput_fd);
            exit(1);
        };

        // Configure the uinput device with virtual keys
        // TODO: what is ioctl?
        ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
        ioctl(uinput_fd, UI_SET_KEYBIT, KEY_NEXTSONG);
        ioctl(uinput_fd, UI_SET_KEYBIT, KEY_PREVIOUSSONG);
        ioctl(uinput_fd, UI_SET_KEYBIT, KEY_PLAYPAUSE);
        ioctl(uinput_fd, UI_SET_KEYBIT, KEY_VOLUMEUP);
        ioctl(uinput_fd, UI_SET_KEYBIT, KEY_VOLUMEDOWN);

        ioctl(uinput_fd, UI_DEV_CREATE);
        cout << "Created virtual keyboard device: " << sDevice << endl;
    }
    else
    {
        cout << "MPD connection established" << endl;
        cout << "Using MPD mode" << endl;
        mode = 0;

    }

    // Open the keyboard device
    fd = open(device, O_RDONLY);
    if (fd < 0) {
        cerr << "Failed to open the keyboard device." << endl;
        cerr << "Run as root." << endl;
        exit(1);
    };
    cout << "Opened keyboard device: " << device << endl;

    // Create a libevdev device
    evdev = nullptr;
    int ret = libevdev_new_from_fd(fd, &evdev);
    if (ret < 0) {
        cerr << "Failed to create libevdev device." << endl;
        close(fd);
        exit(1);
    };
    cout << "Created libevdev device" << endl;

};

/*
void Player::arguments(int argc, char* argv[])
{
    
    //Check if user wants to use a different keyboard device
    //-k /dev/input/eventX
    //
    //Check if user wants to use a different uinput device
    //-u /dev/uinput or /dev/input/eventX
    //
    //Check if user wants to use a different mode
    //-m mpd or simulated
    //
    //argv[0] holds the name of the program.
    //argv[1] points to the first command line argument and argv[argc-1] points to the last argument.
    

    for(int i = 1; i < argc; i++) // Ignore argv[0]
    {
        cout << argv[i] << endl;
        if(argv[i] == "-k")
        {
            cout << "Using keyboard device: " << argv[i+1] << endl;
            device = argv[i+1];
        }
        else if(argv[i] == "-u")
        {
            cout << "Using uinput device: " << argv[i+1] << endl;
            sDevice = argv[i+1];
        }
        else if(argv[i] == "-m")
        {
            if(argv[i+1] == "mpd")
            {
                cout << "Using MPD mode" << endl;
                mode = 0;
            }
            else if(argv[i+1] == "simulated")
            {
                cout << "Using simulated keypress mode" << endl;
                mode = 1;
            }
        }
    }
}
*/

void Player::writeSimulatedKey(unsigned short int key)
{
    sEv.type = EV_KEY;
    sEv.value = 1;
    sEv.code = key;
    write(uinput_fd, &sEv, sizeof(sEv));
    sEv.value = 0;
    write(uinput_fd, &sEv, sizeof(sEv));
}

int Player::getEvent(input_event &ev)
{
    int res = libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    return res;
}

void Player::next()
{
    cout << "Skipping to the next song..." << endl;
    switch(mode)
    {
        case 0:
            // MPD Mode
            mpd_run_next(conn);
            break;
        case 1:
            // Simulated keypress mode
            writeSimulatedKey(KEY_NEXTSONG);
            break;
    }
};

void Player::previous()
{
    cout << "Skipping to the previous song..." << endl;
    switch(mode)
    {
        case 0:
            // MPD Mode
            mpd_run_previous(conn);
            break;
        case 1:
            // Simulated keypress mode
            writeSimulatedKey(KEY_PREVIOUSSONG);
            break;
    }
}

void Player::play()
{
    cout << "Playing..." << endl;
    switch(mode)
    {
        case 0:
            // MPD Mode
            mpd_run_play(conn);
            break;
        case 1:
            // Simulated keypress mode
            writeSimulatedKey(KEY_PLAYPAUSE);
            break;
    }
};

void Player::volumeUp()
{
    cout << "Increasing volume..." << endl;
    switch(mode)
    {
        case 0:
            // MPD Mode
            mpd_run_change_volume(conn, 5);
            break;
        case 1:
            // Simulated keypress mode
            writeSimulatedKey(KEY_VOLUMEUP);
            break;
    }
};

void Player::volumeDown()
{
    cout << "Decreasing volume..." << endl;
    switch(mode)
    {
        case 0:
            // MPD Mode
            mpd_run_change_volume(conn, -5);
            break;
        case 1:
            // Simulated keypress mode
            writeSimulatedKey(KEY_VOLUMEDOWN);
            break;
    }

};

void Player::free()
{
    cout << "Cleaning up..." << endl;
    libevdev_free(evdev);
    mpd_connection_free(conn);
    ioctl(uinput_fd, UI_DEV_DESTROY); // Destroy virtual device
    close(uinput_fd); // CLose uinput device
    close(fd); // Close keyboard device
}

int main(int argc, char* argv[])
{
    Player player;
    // player.arguments(argc, argv); // Parse command line arguments
    input_event ev;
    int holding;

    while(true) 
    {
        player.getEvent(ev);
        if (ev.type == EV_KEY) 
        {
            if (holding == KEY_LEFTCTRL && ev.code == KEY_RIGHT) {
                player.next();
                continue;
            }
            if (holding == KEY_LEFTCTRL && ev.code == KEY_LEFT) {
                player.previous();
                continue;
            }
            if (holding == KEY_LEFTCTRL && ev.code == KEY_UP) {
                player.volumeUp();
                continue;
            }
            if (holding == KEY_LEFTCTRL && ev.code == KEY_DOWN) {
                player.volumeDown();
                continue;
            }

            // Keep track of last key pressed to track multiple keys pressed at once
            if(ev.value == 1)
            {
                holding = ev.code;
            } 
            else if((ev.value == 0) && (ev.code == holding))
            {
                holding = 0;
            }
        }

        usleep(1000);
    }

    player.free();
    return 0;
}