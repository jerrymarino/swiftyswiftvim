# location of the Python header files
 
PYTHON_VERSION = 2.7
PYTHON_INCLUDE = /usr/include/python$(PYTHON_VERSION)
 
# location of the Boost Python include files and library
 
BOOST_INC = /usr/local/include
BOOST_LIB = /usr/local/lib
 
# compile mesh classes
TARGET = hello
 
$(TARGET).so: $(TARGET).o
	clang++ -shared -L$(BOOST_LIB) -lboost_python -lpython$(PYTHON_VERSION) -install_name $(TARGET).so -o $(TARGET).so $(TARGET).o

$(TARGET).o: $(TARGET).cpp
	clang++ -I$(PYTHON_INCLUDE) -I$(BOOST_INC) -fPIC -c $(TARGET).cpp
