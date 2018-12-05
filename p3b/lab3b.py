"""
NAME: Yinsheng Zhou, Jianzhi Liu
EMAIL: jacobzhou@g.ucla.edu, ljzprivate@yahoo.com
ID: 004817743, 204742214
SLIPDAYS: 0
"""
import sys
import csv

class Dir():
    def __init__(self,dirent):
        self.parent_inode = int(dirent[1])
        self.dir_inode = int(dirent[3])
        self.name = dirent[-1]
        self.check_failed = False
    def check(self, path):
        if (self.name == "'.'" and self.parent_inode != self.dir_inode):
            self.check_failed = True
            print("DIRECTORY INODE {0} NAME LINK TO INODE {1} SHOULD BE {2}"\
                  .format(self.parent_inode, self.dir_inode, self.parent_inode))
        if (self.name == "'..'" and self.dir_inode != path[self.parent_inode]):
            self.check_failed = True
            print("DIRECTORY INODE {0} NAME {1} LINK TO INODE {2} SHOULD BE {3}"\
                  .format(self.parent_inode, self.name, self.dir_inode, path[self.parent_inode]))

class Filesystem():
    def __init__(self, summary):
        self.inconsistency_found = False
        self.inode_summary = []
        self.indirect_summary = []
        self.dirs = []
        self.inode_map = {}
        self.block_map = {}
        self.link_map = {}
        for row in summary:
            if row[0] == "SUPERBLOCK":
                self.total_number_of_blocks = int(row[1])
                self.block_size = int(row[3])
                self.inode_size = int(row[4])
                self.first_inode = int(row[7])
            elif row[0] == "GROUP":
                self.nblocks = int(row[2])
                self.ninodes = int(row[3])
                self.first_inode_block_number = int(int(row[8]) +\
                                                self.inode_size*self.ninodes/self.block_size)
            elif row[0] == "INODE":
                self.inode_summary.append(row)
            elif row[0] == "BFREE":
                self.block_map[int(row[1])] = "BFREE"
            elif row[0] == "IFREE":
                self.inode_map[int(row[1])] = "IFREE"
            elif row[0] == "INDIRECT":
                self.indirect_summary.append(row)
            elif row[0] == "DIRENT":
                self.dirs.append(Dir(row))
            else:
                sys.stderr.write("Unrecognized row {}\n".format(row))
                exit(1)
                
    def get_block_status(self, block_number):
        status = "NORMAL"
        if (block_number < 0 or block_number > self.total_number_of_blocks):
            status = "INVALID"
        elif (block_number != 0 and block_number < self.first_inode_block_number):
            status = "RESERVED"
        return status

    def get_block_type_and_offset(self, index, for_indirect=False):
        block_type = "BLOCK"
        offset = 0
        if for_indirect:
            index = index + 11
        if index == 12:
            block_type = "INDIRECT BLOCK"
            offset = 12
        if index == 13:
            block_type = "DOUBLE INDIRECT BLOCK"
            offset = 256 + 12
        if index == 14:
            block_type = "TRIPLE INDIRECT BLOCK"
            offset = 256**2 + 256 + 12
        return block_type, offset

    def check_block_consistency(self):
        for row in self.inode_summary:
            if int(row[11]) == 0 and row[2] == "s": continue
            inode_number = int(row[1])
            index = 0
            for number_str in row[12:27]:
                block_number = int(number_str)
                if block_number == 0: index +=1; continue
                block_type, offset = self.get_block_type_and_offset(index)
                if block_number not in self.block_map:
                    status = self.get_block_status(block_number)
                    if status != "NORMAL":
                        self.inconsistency_found = True
                        print("{0} {1} {2} IN INODE {3} AT OFFSET {4}"\
                              .format(status, block_type, block_number, inode_number, offset))
                    else:
                        self.block_map[block_number] = (inode_number, block_type, offset)
                else:
                    if (self.block_map[block_number] == "BFREE"):
                        self.inconsistency_found = True
                        print("ALLOCATED BLOCK {0} ON FREELIST".format(block_number))
                    else:
                        self.inconsistency_found = True
                        inode_number2, block_type2, offset2 = self.block_map[block_number]
                        print("DUPLICATE {0} {1} IN INODE {2} AT OFFSET {3}"\
                              .format(block_type2, block_number, inode_number2, offset2))
                        print("DUPLICATE {0} {1} IN INODE {2} AT OFFSET {3}"\
                              .format(block_type, block_number, inode_number, offset))
                index += 1

        for row in self.indirect_summary:
            inode_number = int(row[1])
            indirect_level = int(row[2])
            block_number = int(row[-1])
            block_type, offset = self.get_block_type_and_offset(indirect_level, True)
            if block_number not in self.block_map:
                status = self.get_block_status(block_number)
                if status != "NORMAL":
                    self.inconsistency_found = True
                    print("{0} {1} {2} IN INODE {3} AT OFFSET {4}"\
                          .format(status, block_type, block_number, inode_number, offset))
                else:
                    self.block_map[block_number] = (inode_number, block_type, offset)
            else:
                if self.block_map[block_number] == "BFREE":
                    self.inconsistency_found = True
                    print("ALLOCATED BLOCK {0} ON FREELIST".format(block_number))
                else:
                    self.inconsistency_found = True
                    inode_number2, block_type2, offset2 = self.block_map[block_number]
                    print("DUPLICATE {0} {1} IN INODE {2} AT OFFSET {3}"\
                          .format(block_type2, block_number, inode_number2, offset2))
                    print("DUPLICATE {0} {1} IN INODE {2} AT OFFSET {3}"\
                          .format(block_type, block_number, inode_number, offset))
        for block_number in range(self.first_inode_block_number, self.nblocks):
            if block_number not in self.block_map:
                self.inconsistency_found = True
                print("UNREFERENCED BLOCK {}".format(block_number))

    def check_inode_consistency(self):
        for row in self.inode_summary:
            inode_number = int(row[1])
            if inode_number not in self.inode_map:
                self.inode_map[inode_number] = "IUSED"
            else:
                self.inconsistency_found = True
                print("ALLOCATED INODE {0} ON FREELIST".format(inode_number))
                self.inode_map[inode_number] = "IUSED"
        for inode_number in range(self.first_inode, self.ninodes+1):
            if (inode_number not in self.inode_map):
                self.inconsistency_found = True
                print("UNALLOCATED INODE {0} NOT ON FREELIST".format(inode_number))

    def check_dir_consistency(self):
        for inode in self.inode_summary:
            self.link_map[int(inode[1])] = (int(inode[6]),0)#creating a map of inode counter
        path = dict()
        for dirvar in self.dirs:
            #print(dirvar.dir_inode)
            #print(self.inode_map[dirvar.dir_inode])
            if(dirvar.dir_inode in self.inode_map and self.inode_map[dirvar.dir_inode] == "IFREE"):
                self.inconsistency_found = True
                print("DIRECTORY INODE {0} NAME {1} UNALLOCATED INODE {2}"\
                      .format(dirvar.parent_inode, dirvar.name, dirvar.dir_inode))
            elif(dirvar.dir_inode not in self.link_map):
                self.inconsistency_found = True
                print("DIRECTORY INODE {0} NAME {1} INVALID INODE {2}"\
                      .format(dirvar.parent_inode, dirvar.name, dirvar.dir_inode))
            else:
                if(dirvar.dir_inode not in path):
                    path[dirvar.dir_inode] = dirvar.parent_inode
                    #print("current:{} parent:{}".format(dirvar.dir_inode, dirvar.parent_inode))
                self.link_map[dirvar.dir_inode] = (self.link_map[dirvar.dir_inode][0],self.link_map[dirvar.dir_inode][1] + 1)
            dirvar.check(path)
            if dirvar.check_failed:
                self.inconsistency_found = True
        for inode, counts in self.link_map.items():
            count1, count2 = counts
            if(count1 != count2):
                self.inconsistency_found = True
                print("INODE {0} HAS {1} LINKS BUT LINKCOUNT IS {2}"\
                      .format(inode,count2,count1))

def main():
    usage = "Usage: ./lab3b csv_filename\n"
    if (len(sys.argv) !=  2):
        sys.stderr.write(usage)
        exit(1)
    try:
        csvfile = open(sys.argv[1],'r')
    except IOError:
        sys.stderr.write("Failed to open {0}\n".format(sys.argv[1]))
        exit(1)
    summary = csv.reader(csvfile)
    fs = Filesystem(summary)
    fs.check_block_consistency()
    fs.check_inode_consistency()
    fs.check_dir_consistency()
    csvfile.close()
    if fs.inconsistency_found:
        exit(2)
    else:
        exit(0)

if __name__ == "__main__":
    main()
