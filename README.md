# Distributed Imaging Services

This project implements a distributed image processing pipeline in C++. It consists of three loosely coupled applications that communicate exclusively via ZeroMQ (ZMQ) messaging.

App 1 (Image Generator): Reads images from a folder and publishes them.

App 2 (Feature Extractor): Subscribes to images, runs SIFT feature detection on multiple threads using OpenCV, and re-publishes the image + keypoints.

App 3 (Data Logger): Subscribes to the processed data and logs it to an SQLite database.

Dependencies: 

# C++ build tools
sudo apt-get update

sudo apt-get install build-essential cmake pkg-config

# OpenCV
sudo apt-get install libopencv-dev

# ZeroMQ (C++ bindings)
sudo apt-get install libzmq3-dev

# SQLite3
sudo apt-get install libsqlite3-dev


# Build Instructions

Clone the repository:

git clone https://github.com/sahamidfathi/DistributedImagingServices.git

cd DistributedImagingServices

Create a build directory and run cmake and make:

mkdir build

cd build

cmake ..

make -j$(nproc)

This will create three executables in the build/ directory:

image_generator

feature_extractor

data_logger


We need a folder of images to test with such as:

mkdir -p ../images

wget -O ../images/img1.jpg https://placehold.co/1920x1080.jpg?text=Image+1

wget -O ../images/img2.png https://placehold.co/800x600.png?text=Image+2

wget -O ../images/img3.jpg https://placehold.co/1280x720.jpg?text=Image+3

# Running 

The applications can be started in any order. Open three separate terminals.

Terminal 1: Run the Data Logger

cd /path/to/DistributedImagingServices/build

./data_logger

Terminal 2: Run the Feature Extractor

cd /path/to/DistributedImagingServices/build

./feature_extractor

Terminal 3: Run the Image Generator. Provide path to your image directory, if omitted, defaults to ../images relative to the build dir.

cd /path/to/DistributedImagingServices/build

./image_generator ../images

While the apps are running, onc can check the database in another terminal.

Install sqlite3 command-line tool

sudo apt-get install sqlite3

Check the logger's output database

sqlite3 ./build/processed_data.db


Inside the SQLite prompt:

SELECT COUNT(*) FROM processed_images;

SELECT id, filename, timestamp FROM processed_images ORDER BY id DESC LIMIT 5;

