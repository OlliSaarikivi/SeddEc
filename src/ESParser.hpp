#pragma once

struct Event {
    unsigned id;
    vector<unsigned> predecessors;
    vector<unsigned> conflicts;
};

vector<Event> ParseEventStructure(string);