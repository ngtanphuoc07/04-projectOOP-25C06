#pragma once
#include <iostream>
using namespace std;
class Category{
private:
    string categoryID, categoryName;
public:
    void setID(string);
    string getId();
    void setName(string);
    string getName();
};