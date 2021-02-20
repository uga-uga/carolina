#pragma once

#include <string>

using std::string;

#include <vector>

using std::vector;

#include "channel.hpp"

struct Detector{
    const string name;
    const vector<Channel> channels;
};