#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cmath>

using namespace std;

class Record {
public:
    int id, manager_id; // Employee ID and their manager's ID
    string bio, name; // Fixed length string to store employee name and biography

    Record(vector<string> &fields) {
        id = stoi(fields[0]);
        name = fields[1];
        bio = fields[2];
        manager_id = stoi(fields[3]);
    }
	
	// Function to get the size of the record
    int get_size() {
        // sizeof(int) is for name/bio size() in serialize function
        return sizeof(id) + sizeof(manager_id) + sizeof(int) + name.size() + sizeof(int) + bio.size(); 
    }

    // Function to serialize the record for writing to file
    string serialize() const {
        ostringstream oss;
        oss.write(reinterpret_cast<const char *>(&id), sizeof(id));
        oss.write(reinterpret_cast<const char *>(&manager_id), sizeof(manager_id));
        int name_len = name.size();
        int bio_len = bio.size();
        oss.write(reinterpret_cast<const char *>(&name_len), sizeof(name_len));
        oss.write(name.c_str(), name.size());
        oss.write(reinterpret_cast<const char *>(&bio_len), sizeof(bio_len));
        oss.write(bio.c_str(), bio.size());
        return oss.str();
    }

    void print() {
        cout << "\tID: " << id << "\n";
        cout << "\tNAME: " << name << "\n";
        cout << "\tBIO: " << bio << "\n";
        cout << "\tMANAGER_ID: " << manager_id << "\n";
    }
};

class Page {
public:
    vector<Record> records; // Data_Area containing the records
    vector<pair<int, int>> slot_directory; // Slot directory containing offset and size of each record
    int cur_size = sizeof(int); // Current size of the page including the overflow page pointer. if you also write the length of slot directory change it accordingly.
    int overflowPointerIndex;  // Initially set to -1, indicating the page has no overflow page. 
							   // Update it to the position of the overflow page when one is created.

    // Constructor
    Page() : overflowPointerIndex(-1) {}

    // Function to insert a record into the page
    bool insert_record_into_page(Record r) {
        int record_size = r.get_size();
        int slot_size = sizeof(int) * 2;
        if (cur_size + record_size + slot_size > 4096) { // Check if page size limit exceeded, considering slot directory size
            return false; // Cannot insert the record into this page
        } else {
            int record_offset = cur_size;
            records.push_back(r);
            cur_size += record_size + slot_size;
            slot_directory.push_back({record_offset, record_size});
            return true;
        }
    }

    // Function to write the page to a binary output stream. You may use
    void write_into_data_file(ostream &out) const {
        char page_data[4096] = {0}; // Buffer to hold page data
        int offset = 0;

        // Write records into page_data buffer
        for (const auto &record: records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        // TODO:
        //  - Write slot_directory in reverse order into page_data buffer.
        //  - Write overflowPointerIndex into page_data buffer.
        //  You should write the first entry of the slot_directory, which have the info about the first record at the bottom of the page, before overflowPointerIndex.

        int slot_offset = 4096;
        slot_offset -= sizeof(int);
        memcpy(page_data + slot_offset, &overflowPointerIndex, sizeof(int));
        for (int i = slot_directory.size() - 1; i >= 0; i--) {
            slot_offset -= sizeof(int);
            memcpy(page_data + slot_offset, &slot_directory[i].second, sizeof(int));
            slot_offset -= sizeof(int);
            memcpy(page_data + slot_offset, &slot_directory[i].first, sizeof(int)); 
        }

        // Write the page_data buffer to the output stream
        out.write(page_data, sizeof(page_data));
    }

    // Function to read a page from a binary input stream
    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0}; // Buffer to hold page data
        in.read(page_data, 4096); // Read data from input stream

        streamsize bytes_read = in.gcount();
        if (bytes_read == 4096) {
            // TODO: Process data to fill the records, slot_directory, and overflowPointerIndex
            records.clear();
            slot_directory.clear();

            // Read overflow pointer
           int slot_offset = 4096 - sizeof(int);  // overflow pointer
memcpy(&overflowPointerIndex, page_data + slot_offset, sizeof(int));

slot_offset -= sizeof(int);

while (slot_offset >= 0) {
    int size, offset;

    // check bounds before reading
    if (slot_offset - sizeof(int) < 0) break;
    memcpy(&size, page_data + slot_offset, sizeof(int));
    slot_offset -= sizeof(int);

    if (slot_offset - sizeof(int) < 0) break;
    memcpy(&offset, page_data + slot_offset, sizeof(int));
    slot_offset -= sizeof(int);

    // sanity check
    if (offset < 0 || offset >= 4096 || size <= 0 || size > 4096) break;

    slot_directory.push_back({offset, size});

    // optional: stop if offset/size is zero
    if (offset == 0 && size == 0) break;
}

            for (auto &entry : slot_directory) {
                if (entry.first < 0 || entry.first + entry.second > 4096) continue;
                const char *p = page_data + entry.first;

                int id = *reinterpret_cast<const int*>(p); p += sizeof(int);
                int manager_id = *reinterpret_cast<const int*>(p); p += sizeof(int);
                int name_len = *reinterpret_cast<const int*>(p); p += sizeof(int);
                string name(p, name_len); p += name_len;
                int bio_len = *reinterpret_cast<const int*>(p); p += sizeof(int);
                string bio(p, bio_len);

                vector<string> fields = {to_string(id), name, bio, to_string(manager_id)};
                records.push_back(Record(fields));
            }
            return true;
        }

        if (bytes_read > 0) {
            cerr << "Incomplete read: Expected 4096 bytes, but only read " << bytes_read << " bytes." << endl;
        }

        return false;
    }
};

class HashIndex {
private:
    const size_t maxCacheSize = 1; // Maximum number of pages in the buffer
    const int Page_SIZE = 4096; // Size of each page in bytes
    vector<int> PageDirectory; // Map h(id) to a bucket location in EmployeeIndex(e.g., the jth bucket)
    // can scan to correct bucket using j*Page_SIZE as offset (using seek function)
    // can initialize to a size of 256 (assume that we will never have more than 256 regular (i.e., non-overflow) buckets)
    int nextFreePage; // Next place to write a bucket
    string fileName;

    // Function to compute hash value for a given ID
    int compute_hash_value(int id) {
        int hash_value;
        // TODO: Implement the hash function h = id mod 2^8
        hash_value = id % 256;
        return hash_value;
    }

    // Function to add a new record to an existing page in the index file
    void addRecordToIndex(int pageIndex, Page &page, Record &record) {
        // Open index file in binary mode for updating
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);

        if (!indexFile) {
            cerr << "Error: Unable to open index file for adding record." << endl;
            return;
        }
		
		// TODO: 
        //  - Use seekp() to seek to the offset of the correct page in the index file
		//		indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
		//  - try insert_record_into_page()
		//     - if it fails, then you'll need to either...
		//			- go to next overflow page and try inserting there (keep doing this until you find a spot for the record)
		//			- create an overflow page (if page.overflowPointerIndex == -1) using nextFreePage. update nextFreePage index and pageIndex.


        // Seek to the appropriate position in the index file
		// TODO: After inserting the record, write the modified page back to the index file. 
		//		 Remember to use the correct position (i.e., pageIndex) if you are writing out an overflow page!
        while (true) {
            Page page;
            indexFile.seekg(pageIndex * Page_SIZE, ios::beg);
            page.read_from_data_file(indexFile);
            if (page.insert_record_into_page(record)) {
                indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
                page.write_into_data_file(indexFile);
                indexFile.close();
                return;
            }
            if (page.overflowPointerIndex == -1) {
                page.overflowPointerIndex = nextFreePage++; // Create new overflow page
                indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
                page.write_into_data_file(indexFile);
                pageIndex = page.overflowPointerIndex;
            } else {
                pageIndex = page.overflowPointerIndex;
            }
        }

    }

    // Function to search for a record by ID in a given page of the index file
    void searchRecordByIdInPage(int pageIndex, int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // Seek to the appropriate position in the index file
        indexFile.seekg(pageIndex * Page_SIZE, ios::beg);

        // Read the page from the index file
        Page page;
        page.read_from_data_file(indexFile);

        // TODO:
        //  - Search for the record by ID in the page
        //  - Check for overflow pages and report if record with given ID is not found

        for (auto &r : page.records) {
        if (r.id == id) {
            cout << "Employee found in page " << pageIndex << ":\n";
            r.print();
            return;
            }   
        }

        int next = page.overflowPointerIndex;

        while (next != -1) {
            indexFile.seekg(next * Page_SIZE, ios::beg);
            Page overflowPage;
            overflowPage.read_from_data_file(indexFile);
            for (auto &r : overflowPage.records) {
                if (r.id == id) {
                    cout << "Employee found in overflow page " << next << ":\n";
                    r.print();
                    return;
                }
            }

            next = overflowPage.overflowPointerIndex;
        }

        cout << "Employee with ID " << id << " not found.\n";

    }

public:
    HashIndex(string indexFileName) : nextFreePage(0), fileName(indexFileName) {
    }

    // Function to create hash index from Employee CSV file
    void createFromFile(string csvFileName) {
        // Read CSV file and add records to index
        // Open the CSV file for reading
        ifstream csvFile(csvFileName);

        string line;
        // Read each line from the CSV file
        while (getline(csvFile, line)) {
            // Parse the line and create a Record object
            stringstream ss(line);
            string item;
            vector<string> fields;
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            Record record(fields);

            // TODO:
            //   - Compute hash value for the record's ID using compute_hash_value() function.
            //   - Get the page index from PageDirectory. If it's not in PageDirectory, define a new page using nextFreePage.
            //   - Insert the record into the appropriate page in the index file using addRecordToIndex() function.

            int hashValue = compute_hash_value(record.id);
            if (hashValue >= PageDirectory.size()) {
                PageDirectory.resize(hashValue + 1, -1);
            }
            int pageIndex;
            if (PageDirectory[hashValue] == -1) {
                pageIndex = nextFreePage++;
                PageDirectory[hashValue] = pageIndex;
                ofstream indexFile(fileName, ios::binary | ios::in | ios::out | ios::app);
                Page newPage;
                indexFile.seekp(pageIndex * 4096, ios::beg);
                newPage.write_into_data_file(indexFile);
                indexFile.close();
            } else {
                pageIndex = PageDirectory[hashValue];
            }
            Page tempPage;
            addRecordToIndex(pageIndex, tempPage, record);
        }

        // Close the CSV file
        csvFile.close();
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // TODO:
        //  - Compute hash value for the given ID using compute_hash_value() function
        //  - Search for the record in the page corresponding to the hash value using searchRecordByIdInPage() function
        int h = compute_hash_value(id);
        if (h >= PageDirectory.size() || PageDirectory[h] == -1) {
            cout << "Employee not found\n";
            return;
        }

        int pageIndex = PageDirectory[h];

        if (pageIndex == -1) {
            cout << "Employee not found\n";
            return;
        }

        searchRecordByIdInPage(pageIndex, id);
        // Close the index file
        indexFile.close();
    }
};

