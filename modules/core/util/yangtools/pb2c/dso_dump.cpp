
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <iostream>
#include <fstream>
#include <string>
using namespace std;
//#include <stdio.h>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/compiler/parser.h>

using namespace google::protobuf;

//-----------------------------------------------------------------------------
// Parsing given .proto file for Descriptor of the given message (by
// name).  The returned message descriptor can be used with a
// DynamicMessageFactory in order to create prototype message and
// mutable messages.  For example:
/*
  DynamicMessageFactory factory;
  const Message* prototype_msg = factory.GetPrototype(message_descriptor);
  const Message* mutable_msg = prototype_msg->New();
*/
//-----------------------------------------------------------------------------
void GetMessageTypeFromProtoFile(const string& proto_filename,
                                 FileDescriptorProto * file_desc_proto) {
  using namespace google::protobuf::io;
  using namespace google::protobuf::compiler;

  FILE* proto_file = fopen(proto_filename.c_str(), "r");
  {
    if (proto_file == NULL) {
      cout << "Cannot open .proto file: " << proto_filename;
      exit(-1);
    }

    FileInputStream proto_input_stream(fileno(proto_file));
    Tokenizer tokenizer(&proto_input_stream, NULL);
    Parser parser;
    if (!parser.Parse(&tokenizer, file_desc_proto)) {
      cout << "Cannot parse .proto file:" << proto_filename;
      exit(-1);
    }
  }
  fclose(proto_file);

  // Here we walk around a bug in protocol buffers that
  // |Parser::Parse| does not set name (.proto filename) in
  // file_desc_proto.
  if (!file_desc_proto->has_name()) {
    file_desc_proto->set_name(proto_filename);
  }
}


//-----------------------------------------------------------------------------
// Print contents of a record file with following format:
//
//   { <int record_size> <KeyValuePair> }
//
// where KeyValuePair is a proto message defined in mpimr.proto, and
// consists of two string fields: key and value, where key will be
// printed as a text string, and value will be parsed into a proto
// message given as |message_descriptor|.
//-----------------------------------------------------------------------------
void PrintDataFile(const string& data_filename,
                   const FileDescriptorProto& file_desc_proto,
                   const string& message_name) {
#define kMaxRecieveBufferSize (32 * 1024 * 1024)  // 32MB
  static char buffer[kMaxRecieveBufferSize];

  ifstream input_stream(data_filename.c_str());
  if (!input_stream.is_open()) {
    cout << "Cannot open data file: " << data_filename;
    exit(-1);
  }

  google::protobuf::DescriptorPool pool;
  const google::protobuf::FileDescriptor* file_desc =
    pool.BuildFile(file_desc_proto);
  if (file_desc == NULL) {
    cout << "Cannot get file descriptor from file descriptor"
               << file_desc_proto.DebugString();
    exit(-1);
  }

  const google::protobuf::Descriptor* message_desc =
    file_desc->FindMessageTypeByName(message_name);
  if (message_desc == NULL) {
    cout << "Cannot get message descriptor of message: " << message_name;
    exit(-1);
  }

  google::protobuf::DynamicMessageFactory factory;
  const google::protobuf::Message* prototype_msg =
    factory.GetPrototype(message_desc);
  if (prototype_msg == NULL) {
    cout << "Cannot create prototype message from message descriptor";
    exit(-1);
  }
  google::protobuf::Message* mutable_msg = prototype_msg->New();
  if (mutable_msg == NULL) {
    cout << "Failed in prototype_msg->New(); to create mutable message";
    exit(-1);
  }

  uint32 proto_msg_size;
  input_stream.seekg(0,ios::end);
  proto_msg_size = input_stream.tellg();
  if (proto_msg_size > kMaxRecieveBufferSize) {
    cout << "Failed to read a proto message with size = "
	 << proto_msg_size
	 << ", which is larger than kMaxRecieveBufferSize ("
	 << kMaxRecieveBufferSize << ")."
	 << "You can modify kMaxRecieveBufferSize defined in "
	 << __FILE__;
    exit(-1);
  }

  input_stream.seekg(0,ios::beg);
  for (;;) {
    input_stream.read(buffer, proto_msg_size);
    if (!input_stream)
      break;

    if (!mutable_msg->ParseFromArray(buffer, proto_msg_size)) {
      cout << "Failed to parse value in KeyValuePair:" ; // HACK << pair.value();
      exit(-1);
    }

    cout << mutable_msg->DebugString();
  }

  delete mutable_msg;
}

int main(int argc, char** argv) {
  string proto_filename = "gen/descriptor.proto", message_name = "FileDescriptorSet";
  string file1;
  vector<string> data_filenames;
  FileDescriptorProto file_desc_proto;

  if (argc != 2) {
    cout << "Usage: " << argv[0] << " <filename.dso>\n";
  }
  else {
    file1 = string(argv[1]);
  }


  //  ParseCmdLine(argc, argv, &proto_filename, &message_name, &data_filenames);
  GetMessageTypeFromProtoFile(proto_filename, &file_desc_proto);

#if 1
  data_filenames.push_back(file1);

  for (uint32 i = 0; i < data_filenames.size(); ++i) {
    PrintDataFile(data_filenames[i], file_desc_proto, message_name);
  }
#endif

  exit(0);
}
