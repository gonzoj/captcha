all:
	g++ -Wall -g -o captcha.so -Wl,-E -fPIC -shared -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_ml core.cpp

