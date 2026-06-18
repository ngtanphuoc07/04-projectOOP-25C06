#include <iostream>
#include "Category.h"
using namespace std;

void Category::setID(string id){
    this->categoryID = id;
}

string Category::getId(){
    return categoryID; 
}

void Category::setName(string name){
    this->categoryName = name;
}

string Category::getName(){
    return categoryName;
}