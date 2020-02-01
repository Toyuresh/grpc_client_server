from conans import ConanFile, CMake


class CppdummyConan(ConanFile):
    name = "cppdummy"
    version = "0.1"
    license = "<Put the package license here>"
    author = "<Put your name here> <And your email here>"
    url = "<Package recipe repository url here, for issues about the package>"
    description = "<Description of Cppdummy here>"
    topics = ("<Put some tag here>", "<here>", "<and here>")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": False}
    generators = "cmake"
    exports_sources = "src/*"
    requires = (("protobuf/3.9.1@bincrafters/stable"),
                ("protoc_installer/3.9.1@bincrafters/stable"),
                ("grpc/1.25.0@inexorgame/stable"),
                )
    

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder="src")
        cmake.build()

        # Explicit way:
        # self.run('cmake %s/hello %s'
        #          % (self.source_folder, cmake.command_line))
        # self.run("cmake --build . %s" % cmake.build_config)

    def package(self):
        self.copy("*.hpp",dst="inlcude",src="src")
        self.copy("*.h", dst="include",keep_path=False)
        self.copy("*.cc",dst="include",keep_path=False)
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.dylib*", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)
        self.copy("*.json", dst="bin", keep_path=False)
        self.copy("*greeter_client",dst="bin",keep_path=False)
        self.copy("*greeter_server",dst="bin",keep_path=False)
        self.copy("*greeter_async_client",dst="bin",keep_path=False)
        self.copy("*greeter_async_server",dst="bin",keep_path=False)
        self.copy("*route_guide_client",dst="bin",keep_path=False)
        self.copy("*route_guide_server",dst="bin",keep_path=False)
        self.copy("*route_guide_client_async",dst="bin",keep_path=False)
        self.copy("*route_guide_server_async",dst="bin",keep_path=False)
        self.copy("*math_client",dst="bin",keep_path=False)
        self.copy("*math_server",dst="bin",keep_path=False)
        self.copy("*math_client_async",dst="bin",keep_path=False)
        self.copy("*math_server_async",dst="bin",keep_path=False)
        self.copy("*greeter_streaming_server_async",dst="bin",keep_path=False)
        self.copy("*greeter_streaming_server_alarm_async",dst="bin",keep_path=False)
        self.copy("*greeter_streaming_server_queue_to_back",dst="bin",keep_path=False)
        self.copy("*greeter_streaming_server_queue_to_front",dst="bin",keep_path=False)
        self.copy("*greeter_streaming_client",dst="bin",keep_path=False)
        self.copy("*math_streaming_client_async",dst="bin",keep_path=False)
        self.copy("*math_streaming_server_async",dst="bin",keep_path=False)
        self.copy("*dummyServer",dst="bin",keep_path=False)
        self.copy("*dummyClient",dst="bin",keep_path=False)
        self.copy("*chatDemoServer",dst="bin",keep_path=False)
        self.copy("*chatDemoClient",dst="bin",keep_path=False)
        self.copy("*chatDemoSimpleServer",dst="bin",keep_path=False)
    #def package_info(self):
        #self.cpp_info.libs = ["hello"]
