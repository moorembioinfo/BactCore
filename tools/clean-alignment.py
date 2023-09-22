import argparse

def clean_align(input_file):
    output_file_name = f"{input_file.split('.')[0]}-cleaned.align.fasta"
    
    with open(input_file, 'r') as filename, open(output_file_name, 'w') as output:
        line_buffer = []
        for line in filename:
            line_buffer.append(line)
            if len(line_buffer) == 10:
                for buffered_line in line_buffer:
                    if buffered_line.startswith('>'):
                        output.write(buffered_line)
                        print(buffered_line.strip())
                    else:
                        buffered_line = buffered_line.rstrip()
                        for x in buffered_line:
                            if x not in list('ATCGN-'):
                                output.write('N')
                            else:
                                output.write(x)
                        output.write('\n')
                line_buffer = []
        
        # Deal with remaining lines
        for buffered_line in line_buffer:
            if buffered_line.startswith('>'):
                output.write(buffered_line)
                print(buffered_line.strip())
            else:
                buffered_line = buffered_line.rstrip()
                for x in buffered_line:
                    if x not in list('ATCGN-'):
                        output.write('N')
                    else:
                        output.write(x)
                output.write('\n')

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Clean alignment of non ATCGN- chars.')
    parser.add_argument('input_file', type=str, help='Input FASTA.')
    
    args = parser.parse_args()
    clean_align(args.input_file)
