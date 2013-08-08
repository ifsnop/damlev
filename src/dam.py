"""
Compute the Damerau-Levenshtein distance between two given
strings (s1 and s2)
"""
def damerau_levenshtein_distance(s1, s2):
    d = {}
    lenstr1 = len(s1)
    lenstr2 = len(s2)
    for i in xrange(-1,lenstr1+1):
        d[(i,-1)] = i+1
    for j in xrange(-1,lenstr2+1):
        d[(-1,j)] = j+1

    for i in xrange(-1,lenstr1+1):
	for j in xrange(-1,lenstr2+1):
	    if d.has_key((i,j)):
		#print "(" + str(i) + "," + str(j) + ") " + str(d[(i,j)]),
		print "(%02d,%02d) %01d" % (i, j, d[(i,j)]),
	    else:
		print "(%02d,%02d) %s" % (i, j, "-"),
	print
 

    for i in xrange(lenstr1):
        for j in xrange(lenstr2):
            if s1[i] == s2[j]:
                cost = 0
            else:
                cost = 1
            d[(i,j)] = min(
                           d[(i-1,j)] + 1, # deletion
                           d[(i,j-1)] + 1, # insertion
                           d[(i-1,j-1)] + cost, # substitution
                          )
            if i and j and s1[i]==s2[j-1] and s1[i-1] == s2[j]:
                d[(i,j)] = min (d[(i,j)], d[i-2,j-2] + cost) # transposition

    print "====================================================================="

    for i in xrange(-1,lenstr1+1):
	for j in xrange(-1,lenstr2+1):
	    if d.has_key((i,j)):
		#print "(" + str(i) + "," + str(j) + ") " + str(d[(i,j)]),
		print "[(%02d,%02d) %01d]" % (i, j, d[(i,j)]),
	    else:
		print "[(%02d,%02d) %s]" % (i, j, "-"),
	print

 
    return d[lenstr1-1,lenstr2-1]


ret = damerau_levenshtein_distance('de', 'sistema')

print ret


