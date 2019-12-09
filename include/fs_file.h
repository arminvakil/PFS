/*
 * ClientFile.h
 *
 *  Created on: Nov 28, 2019
 *      Author: armin
 */

#ifndef COMMON_FSFILE_H_
#define COMMON_FSFILE_H_

#include <iostream>
#include <vector>
#include <unordered_map>

#include "config.h"
#include "file.h"

class FSFile : public File {
	std::unordered_map<int, std::string> stripPaths;
public:
	FSFile(std::string name, int stripeWidth);
	virtual ~FSFile();

	void addStrip(std::string path, int index);

	std::string getStripPath(int index);
};

#endif /* COMMON_FSFILE_H_ */
