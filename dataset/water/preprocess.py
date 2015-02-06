############################################################
##    FILENAME:   preprocess.py    
##    VERSION:    1.0
##    SINCE:      2015-01-20
##    AUTHOR: 
##        Jimmy Lin (xl5224) - JimmyLin@utexas.edu  
##
############################################################
##    Edited by MacVim
##    Documentation auto-generated by Snippet 
############################################################

usage = '''
    Usage: 
        python preprocess [raw-data] [unscaled-libsvm-data]
'''

import sys

if len(sys.argv) < 2:
    print usage
    sys.exit(0)

infile = open(sys.argv[1], 'rb')
outfile = open(sys.argv[2], 'wb')

data = []
sums = None
counts = None

nRows = 0
docdict = {}
counter = 1
for line in infile:
    features = line.split(",")
    if sums is None:
        sums = [0] * len(features)
    if counts is None:
        counts = [0] * len(features)

    datum = []
    category = features[0].split("/")[1] + "/"+ features[0].split("/")[2]
    if not docdict.has_key(category):
        docdict.update({category:counter})
        counter += 1

    datum.append(str(docdict[category]))
    for i in range(1, len(features)):
        datum.append(features[i])
        print features[i]
        if '?' in features[i] :
            datum.append('1')
        else:
            datum.append('0')
            sums[i] += float(features[i])
            counts[i] += 1.0

    data.append(datum)
    nRows += 1

means = [0] * len(sums)
for i in range(1, len(sums)):
    means[i] = sums[i] / counts[i]
    
for i in range(0, nRows):
   #data[i][0] = int(data[i][0])
   outfile.write(data[i][0] + ' ')
   for j in range(1, len(sums)):
       if data[i][j] == '?':
           data[i][j] = means[j]
       else:
           data[i][j] = float(data[i][j])
       if data[i][j] < 1e-6 and data[i][j] > -1e-6:
           pass
       else:
           outfile.write(str(j) + ":"+ str(data[i][j]) + " ")
   outfile.write('\n')
   outfile.flush()


outfile.close()
infile.close()