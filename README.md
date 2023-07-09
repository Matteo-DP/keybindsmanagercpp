## Simple keybinds manager in C++  
Records keypresses   
Customisable keybinds to execute commands  
### Default keybindings:  
| Keybind | Command | Description |
| ------- | ------- | ----------- |
| LCTRL + ARROW_RIGHT | playerctl next | Skips to the next song |
| LCTRL + ARROW_LEFT | playerctl previous | Skips to the previous song |
| LCTRL + SPACE | playerctl play-pause | Increases volume |
| LCTRL + DEL | sudo shutdown -h now | Exits VIM (not included by default) |
### Arguments:
| Argument | Description | Example |
| -------- | ----------- | ------- |
| --debug | Enables debug mode |
| -d | Specify a device to listen to eventX format | -d event1 |
### Prerequisites:
- Any C++ compiler such as G++ or Clang  
- evdev   
- playerctl to execute the default keybinds (optional)  
### Installation:  
`git clone https://github.com/Matteo-DP/keybindsmanagercpp.git`  
`cd keybindsmanagercpp`  
`g++ main.cpp -o keybinds -levdev`  
`sudo ./keybinds -d eventX`
### Notes:
- Currently only supports linux  
- New keybinds should be specified in the "keybinds.json" file  
    - **! The modifier property is unused !**
#### Format:  
This example triggers `echo hello world` when lctrl and enter is pressed
```
[
    ...,
    {   
        "keybind": [    
            {
                "key": 28,
                "modifier": 0
            },
            {
                "key": 29,
                "modifier": 0
            }
        ],
        "command": "echo Hello World"
    },
    ...,
]  
```
You can find you own key values via evtest  
For example:  
`sudo evtest /dev/input/eventX`  
### Future ideas:
If I ever revisit this project, these are some things that might be added in the future:
- Run the program as a service on the background
    - Automatically reload cache when a change in *keybinds.json* is detected
    - The ability to manage keybinds through CLI
- A lot of refractoring  
### Final note:
This is my first proper C++ program, if you would like to proof read it I am open to all tips!