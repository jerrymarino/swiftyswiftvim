# location of the Python header files
 
PYTHON_VERSION = 2.7
 
# location of the Boost Python include files and library
 
BOOST_INC = /usr/local/include
BOOST_LIB = /usr/local/lib
 
# compile mesh classes
TARGET = hello
 
# This needs to include the python version in which 
# the program will be ran with!
$(TARGET).so: $(TARGET).o
	clang++ -shared -L$(BOOST_LIB)  -isystem /usr/local/Frameworks -F /usr/local/Frameworks -framework Python -I/usr/local/Frameworks/Python.framework/Headers/ -lboost_python -lpython$(PYTHON_VERSION) -install_name $(TARGET).so -o $(TARGET).so $(TARGET).o

$(TARGET).o: $(TARGET).cpp
	clang++ -isystem /usr/local/Frameworks -F /usr/local/Frameworks -I/usr/local/Frameworks/Python.framework/Headers/ -I$(BOOST_INC) -fPIC -c $(TARGET).cpp
