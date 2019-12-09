/*
 * client.cpp
 *
 *  Created on: Nov 26, 2019
 *      Author: armin
 */

#include <cassert>
#include <algorithm>
#include <iostream>
#include <fs_file.h>
#include <cstdio>

FSFile::FSFile(std::string name, int stripeWidth) {
	this->setStripWidth(stripeWidth);
	this->setName(name);
}

FSFile::~FSFile() {
}

void FSFile::addStrip(std::string path, int index) {
	stripPaths[index] = path;
	FILE* file = fopen(path.c_str(), "w");
	fclose(file);
}

std::string FSFile::getStripPath(int index) {
	auto it = stripPaths.find(index);
	if(it == stripPaths.end()) {
		return "";
	}
	return it->second;
}
