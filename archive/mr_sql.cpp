#include "mr_sql.h"

using namespace mrutils;

MYSQL SqlConnect::mysql;
bool SqlConnect::init_ = false;
bool SqlConnect::connected_ = false;
bool SqlConnect::failed_ = false;
MUTEX SqlConnect::sqlMutex = mrutils::mutexCreate();

std::string SqlConnect::server = "localhost";
std::string SqlConnect::username = "simulate";
std::string SqlConnect::password = "simulate";
