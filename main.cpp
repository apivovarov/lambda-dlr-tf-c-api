#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/platform/Environment.h>
#include <aws/lambda-runtime/runtime.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <dlr.h>
#include <sys/time.h>
#include <fstream>
#include <iostream>

int init();

int run_inference();

std::string prepare_resp();

double ms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

bool file_exists(const char* name) {
  std::ifstream f(name);
  return f.good();
}
int download_file(Aws::S3::S3Client const& client, Aws::String const& bucket,
                  Aws::String const& key, std::string dest_file);

DLRModelHandle handle;

char const TAG[] = "LAMBDA_ALLOC";
const char* s3_region = "us-west-1";
const char* s3_bucket = "pivovaa-us-west-1";
const char* s3_pb_file = "autodesk/output_graph_fc3_add.pb";
const char* pb_file = "/tmp/model.pb";

int threads = 0;  // undefined

const char* input_name = "Placeholder_1:0";
const char* output_name = "FC3/add:0";
const int n_dims = 5;
const int64_t dims[5] = {1, 20, 256, 256, 1};
const size_t n_in_elem = 20 * 256 * 256;
const size_t n_out_elem = 1024;

float* input_data;
float* output_data;

int download_file(Aws::S3::S3Client const& client, Aws::String const& bucket,
                  Aws::String const& key, std::string dest_file) {
  Aws::S3::Model::GetObjectRequest request;
  request.WithBucket(bucket).WithKey(key);

  auto outcome = client.GetObject(request);
  if (!outcome.IsSuccess()) {
    printf("ERROR: S3: outcome.IsSuccess is Fasle\n");
    return -1;
  }
  // Get an Aws::IOStream reference to the retrieved file
  auto& retrieved_file = outcome.GetResultWithOwnership().GetBody();

  std::ofstream output_file(dest_file, std::ios::binary);
  output_file << retrieved_file.rdbuf();
  return 0;
}

int init() {
  printf("Initializing\n");
  const DLR_TFTensorDesc inputs[1] = {{input_name, dims, n_dims}};
  const char* outputs[1] = {output_name};

  if (CreateDLRModelFromTensorflow(&handle, pb_file, inputs, 1, outputs, 1,
                                   threads)) {
    printf("ERROR: %s\n", DLRGetLastError());
    return -1;
  }
  printf("CreateDLRModelFromTensorflow - OK\n");

  input_data = (float*)malloc(n_in_elem * sizeof(float));
  output_data = (float*)malloc(n_out_elem * sizeof(float)); 

  // dummy input data
  std::fill_n(input_data, n_in_elem, 0.1);
  std::fill_n(output_data, n_out_elem, 0.0);
  return 0;
}

int run_inference() {
  if (SetDLRInput(&handle, input_name, dims, input_data, n_dims)) {
    printf("ERROR: SetDLRInput failed\n");
    return -1;
  }
  printf("SetDLRInput - OK\n");

  if (RunDLRModel(&handle)) {
    printf("ERROR: RunDLRModel failed\n");
    return -1;
  }
  printf("RunDLRModel - OK\n");

  if (GetDLROutput(&handle, 0, output_data)) {
    printf("ERROR: GetDLROutput failed\n");
    return -1;
  }
  printf("GetDLROutput - OK\n");

  return 0;
}

std::string prepare_resp() {
  bool fexists = file_exists(pb_file);
  int errC = -1;
  double totalT = 0.0;
  if (fexists) {
    double t1 = ms();
    errC = run_inference();
    double t2 = ms();
    totalT = t2 - t1;
    printf("run_inference done, Time: %2.3f ms\n", t2 - t1);
  }
  char buff[1001] = {0};
  int len =
      snprintf(buff, 1000,
               R"({"model":"%s", "fexists":"%s", "errC":%d, "inference_time_ms":%2.3f})",
               pb_file, fexists ? "True" : "False", errC, totalT);
  std::string resp(buff, len);
  return resp;
}

aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& request) {
  (void)request;
  std::string resp = prepare_resp();
  std::cout << std::endl;
  return aws::lambda_runtime::invocation_response::success(resp,
                                                           "application/json");
}

int main() {
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  {
    Aws::Client::ClientConfiguration config;
    config.region = s3_region;

    auto credentialsProvider =
        Aws::MakeShared<Aws::Auth::EnvironmentAWSCredentialsProvider>(TAG);
    Aws::S3::S3Client s3_client(credentialsProvider, config);

    printf("Downloading model from %s s3://%s/%s\n", s3_region, s3_bucket, s3_pb_file);
    if (download_file(s3_client, s3_bucket, s3_pb_file, pb_file)) {
      printf("ERROR: download model file failed\n");
      ShutdownAPI(options);
      return -1;
    }
    printf("Model downloaded - OK\n");

    if (init()) {
      printf("ERROR: init failed\n");
      ShutdownAPI(options);
      return -1;
    }
    //std::cout << prepare_resp() << std::endl;
    run_handler(my_handler);
  }
  ShutdownAPI(options);
  return 0;
}
