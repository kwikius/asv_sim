# ASV Simulator

This package contains plugins and models for the simulation of surface vessels in Gazebo.  

## Dependencies

You will need a working installation of ROS and Gazebo in order to use this package.
It has been tested with:

- Gazebo version 9.4.1
- ROS Melodic Morenia
- OSX 10.11.6

## Installation

Source your ROS installation and update the Gazebo plugin path:

```bash
# Source the ROS and Gazebo environment (add this to ~/.bash_profile)
$ source /opt/ros/melodic/setup.bash
$ source /usr/local/share/gazebo/setup.sh
```

Create and configure a catkin workspace, clone and build the repo:

```bash
# Create a catkin workspace 
$ mkdir -p <catkin_ws>/src

# Clone dependencies
$ cd <catkin_ws>/src
$ git clone https://github.com/srmainwaring/asv_wave_sim.git

# Clone this repo
$ git clone https://github.com/srmainwaring/asv_sim.git

# Configure and build
$ cd <catkin_ws>
$ catkin config --extend /opt/ros/melodic \
  --cmake-args -DCMAKE_CXX_STANDARD=14 -DCMAKE_BUILD_TYPE=RelWithDebInfo 
$ catkin build

# Source the workspace
$ source devel/setup.bash

# Update the Gazebo plugin path
$ export GAZEBO_PLUGIN_PATH=$(pwd)/devel/lib:$GAZEBO_PLUGIN_PATH

# Check the Gazebo environment variables
$ printenv |grep GAZEBO
```

## Anemometer Sensor

AnemometerSensor to measure wind speed and direction.

### Usage

Add the SDF for the sensor to a `<link>` element of your model.

```xml
<sensor name="anemometer_sensor" type="anemometer">
  <always_on>true</always_on>
  <update_rate>50</update_rate>
  <topic>anemometer</topic>
</sensor>
```

### Published Topics

1. `~/anemometer` (`gazebo::msgs::Param_V`)

  - `time` (`gazebo::msgs::Time`) \
    The simulation time of the observation.

  - `true_wind` (`gazebo::msgs::Vector3d`) \
    The true wind at the link origin.

  - `apparent_wind` (`gazebo::msgs::Vector3d`) \
    The apparent wind at the link origin
    (i.e. true wind adjusted for the link velocity).

### Parameters

1. `<always_on>` (`bool`, default: `false`) \
  Standard `<sensor>` parameter.
  See [SDF documentation](http://sdformat.org/spec?ver=1.6&elem=sensor) for details.

2. `<update_rate>` (`double`, default: `0`) \
  Standard `<sensor>` parameter.
  See [SDF documentation](http://sdformat.org/spec?ver=1.6&elem=sensor) for details.

3. `<topic>` (`string`, default: `~/anemometer`) \
  Standard `<sensor>` parameter.
  See [SDF documentation](http://sdformat.org/spec?ver=1.6&elem=sensor) for details.

## Anemometer Example

To run the example:

```bash
roslaunch asv_sim_gazebo anemometer_demo_world.launch verbose:=true
```

The launch file loads the `RegisterSensorsPlugin` system plugin using
the parameter:  

```xml
  <arg name="extra_gazebo_args" default="--server-plugin libRegisterSensorsPlugin.so" />
```

You should see a world containing a single block at the origin. 
The figure below shows the block falling to demonstate the effect
of motion on apparent wind:

![Anemometer World](https://github.com/srmainwaring/asv_sim/wiki/images/anemometer_world_falling.jpg)

Open the Topic Visualization window and select the `anemometer` topic:

![Anemometer Topic](https://github.com/srmainwaring/asv_sim/wiki/images/anemometer_topic.jpg)

When the block is at rest with axis aligned with the world frame,
the true and apparent wind should be the same. When the block is in motion,
for instance by setting the `z` pose to `100` and letting it fall, the
apparent wind will be adjusted for the object's motion.

![Anemometer Rest](https://github.com/srmainwaring/asv_sim/wiki/images/anemometer_topic_view_rest.jpg)
![Anemometer Falling](https://github.com/srmainwaring/asv_sim/wiki/images/anemometer_topic_view_falling.jpg)


## Build Status

### Develop Job Status

|    | Melodic |
|--- |--- |
| asv_sim | [![Build Status](https://travis-ci.com/srmainwaring/asv_sim.svg?branch=feature%2Fwrsc-devel)](https://travis-ci.com/srmainwaring/asv_sim) |


### Release Job Status

|    | Melodic |
|--- |--- |
| asv_sim | [![Build Status](https://travis-ci.com/srmainwaring/asv_sim.svg?branch=master)](https://travis-ci.com/srmainwaring/asv_sim) |


## License

This is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
[GNU General Public License](LICENSE) for more details.
