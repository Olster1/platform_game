../bin/ctime -begin test.ctm
#IMGUI Dynamic library to link against
#clang++ -shared -install_name @rpath/../bin/libimgui.dylib -I ../imgui ../imgui/imgui*.cpp -o ../bin/libimgui.dylib 

ERRORS_OFF=-Wno-c++11-compat-deprecated-writable-strings
#build without IMGUI and jsut link against it. 
clang++ $ERRORS_OFF -Wl,-rpath,@executable_path/ -I ../shared/ -I ../imgui -I ../libs/gl3w imgui_impl_sdl_gl3.o main.cpp ../libs/gl3w/GL/gl3w.cpp -L../bin -limgui -F../bin -framework SDL2 -framework OpenGl -framework CoreFoundation -o ../bin/game -g -DDEVELOPER_MODE=1
../bin/ctime -end test.ctm