#!/usr/bin/env python3
# From https://github.com/renesas/bin2c

import sys, getopt

def main(argv):
    input_file      = ''
    output_file     = ''
    prefix          = ''

    try:
        opts, args = getopt.getopt(argv, "hi:o:p:",["ifile=","ofile=","prefix="])
    except:
        print("bin2c.py -i <binary file to convert> -o <output C file> -p <output variables prefix>")
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print("bin2c.py -i <binary file to convert> -o <output C file> -p <output variables prefix>")
            sys.exit(0)
        elif opt in ("-i", "--ifile"):
            input_file = arg
        elif opt in ("-o", "--ofile"):
            output_file = arg
        elif opt in ("-p", "--prefix"):
            prefix = arg
    
    print("Converting: " + input_file + " to C array in file " + output_file + " with prefix " + prefix)

    # Open the binary file for reading
    try:
        infile = open(input_file, "rb")
    except:
        print("Couldn't open input file: " + input_file)
        sys.exit(2)

    # Read the contents of the file (ROM files are small)
    bin_file_contents = infile.read()

    # Close the file
    infile.close()

    # Write the C source file
    try:
        outfile = open(output_file, "w")
    except:
        print("Couldn't open output file: " + output_file)
        sys.exit(2)

    # Convert the binary contents into C hex values
    outfile.write("#include \"rom.h\"\n\n")
    outfile.write("const uint32_t " + prefix + "rom_size = " + str(len(bin_file_contents)) + ";\n")
    outfile.write("const uint8_t __attribute__((section(\".flashdata.romdata\"))) " + prefix + "rom[]  = {")
    for x in range(len(bin_file_contents)):
        if (x % 16 == 0):
            outfile.write("\n")
            outfile.write("\t")
        outfile.write("0x" + hex(bin_file_contents[x])[2:].zfill(2) + ", ")

    outfile.write("\n};")
    outfile.close()

    print("File converted")

    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv[1:])
