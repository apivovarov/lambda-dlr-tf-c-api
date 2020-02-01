# Intro
This project shows how to use DLR C API to run Tensorflow models on AWS Lambda

# Build

0. Start EC2 box with AL2018.03 (aka AL1) (glibc 2.17)

1. Build and Intall `aws-lambda-cpp` libs

```
git clone https://github.com/awslabs/aws-lambda-cpp.git
cd aws-lambda-cpp
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INSTALL_PREFIX=~/aws-install

make -j$(nproc)
make install
```

2. Build and Install `aws-sdk-cpp` libs

```
git clone https://github.com/aws/aws-sdk-cpp.git
cd aws-sdk-cpp
mkdir build && cd build
cmake .. \
  -DBUILD_ONLY="s3" \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_UNITY_BUILD=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/aws-install

make -j$(nproc)
make install
```

2. Download `libtensorflow.so` v.1.15.0 built for aws lambda (`-march=ivybridge`)

```
mkdir ~/libtensorflow-lambda
cd libtensorflow-lambda
wget https://pivovaa-us-west-1.s3-us-west-1.amazonaws.com/libtensorflow-ivybridge.tar.gz
tar zxf libtensorflow-ivybridge.tar.gz
rm -rf libtensorflow-ivybridge.tar.gz
```

3. Build DLR with Tensorflow support

```
git clone --recursive https://github.com/neo-ai/neo-ai-dlr
cd neo-ai-dlr
mkdir build && cd build
cmake .. \
  -DWITH_TENSORFLOW_LIB=~/libtensorflow-lambda \
  -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)
```

3. Build this project (lambda-dlr-tf-c-api)

```
git clone https://github.com/apivovarov/lambda-dlr-tf-c-api.git
cd  lambda-dlr-tf-c-api
mkdir lib && cd lib
ln -s ../../neo-ai-dlr/build/lib/libdlr.so
ln -s ../../libtensorflow-lambda/lib/libtensorflow_framework.so.1
ln -s ../../libtensorflow-lambda/lib/libtensorflow.so.1
cd ..

mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=~/aws-install

make
# it will produce lambda-dlr-tf binary
```

5. Package and upload lambda package to s3

```
# Assuming you are still in build folder
# Run packager for binary lambda-dlr-tf
../packager lambda-dlr-tf

# it will produce lambda-dlr-tf.zip containing the following files
  adding: bin/ (stored 0%)
  adding: bin/lambda-dlr-tf (deflated 79%)
  adding: bootstrap (deflated 23%)
  adding: lib/ (stored 0%)
  adding: lib/libtensorflow.so.1 (deflated 73%)
  adding: lib/libtensorflow_framework.so.1 (deflated 67%)
  adding: lib/libdlr.so (deflated 60%)

# Upload lambda-dlr-tf.zip to s3
aws s3 cp lambda-dlr-tf.zip s3://<your_bucket>/<folder>/
```

# Create lambda.

* Amazon S3 link URL - s3://<your_bucket>/<folder>/lambda-dlr-tf.zip
* Runtime - Custom runtime
* Handler - lambda-dlr-tf
* Memory - 3008MB
* Timeout - 1 min

Run Test - you should get the following output

```
{
  "model": "/tmp/model.pb",
  "fexists": "True",
  "errC": 0,
  "inference_time_ms": 478.181
}
```
