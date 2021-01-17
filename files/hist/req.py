#!/usr/bin/env python

import sys, json, sqlite3, struct, math

req = json.load(sys.stdin)
db = sqlite3.connect('data/'+req['db']+'.db')

labels = req['labels'].items()
cols = ''.join( ', '+k for k,xs in labels if len(xs)>1 )

def headtype2fmt(x):
    if x=='f8': return 'd'
    if x=='f4': return 'f'
    if x=='u8': return 'L'
    if x=='i8': return 'l'
    if x=='u4': return 'I'
    if x=='i4': return 'i'
    if x=='u2': return 'H'
    if x=='i2': return 'h'
    if x=='u1': return 'B'
    if x=='i1': return 'b'

def head2fmt(head):
    return ''.join( headtype2fmt(x) for x in head.split(',') )

blob = False
for col in db.execute('pragma table_info(hist)'):
    if col[1]=='_data' and col[2]=='BLOB':
        blob = True
        break

sys.stdout.write('[')
first = True

if blob:
    for h in db.execute(
        'select uniform, edges, _head, _data, var1' + cols + ' from hist' +
        ' inner join axis_dict on hist._axis = axis_dict.rowid' +
        ' where ' +
        ' and '.join(
            '('+(' or '.join('%s="%s"' % (k,x) for x in xs))+')'
             for k,xs in labels)
    ):
        if first: first = False
        else: sys.stdout.write(',')
        sys.stdout.write('[')

        njets = h[4].startswith('Njets_')

        if h[0]: # uniform axis
            axis = json.loads(h[1])
            if njets:
                sys.stdout.write('[')
                sys.stdout.write(','.join(str(x-0.5) for x in xrange(axis[0]+2)))
                sys.stdout.write(']')
            else:
                width = (axis[2]-axis[1])/axis[0]
                sys.stdout.write('[')
                for i in xrange(axis[0]+1):
                    if i: sys.stdout.write(',')
                    sys.stdout.write('{0:.5g}'.format(axis[1] + i*width))
                sys.stdout.write(']')
        else: # explicit edges
            sys.stdout.write(h[1])

        fmt = head2fmt(h[2])
        sys.stdout.write(',[')
        if njets:
            sys.stdout.write('0,0,')
        first_bin = True
        for offset in xrange(0,len(h[3]),struct.calcsize(fmt)):
            if first_bin: first_bin = False
            else: sys.stdout.write(',')
            b = struct.unpack_from(fmt,h[3],offset)
            sys.stdout.write('{0:.5g},{1:.5g}'.format(b[0],math.sqrt(b[1])))
        sys.stdout.write('],\"')
        sys.stdout.write(' '.join('*' if x=='' else x for x in h[5:]))
        sys.stdout.write('\"]')

else:
    for h in db.execute(
        'select edges, bins' + cols + ' from hist' +
        ' inner join axes on hist.axis = axes.id' +
        ' where ' +
        ' and '.join(
            '('+(' or '.join('%s="%s"' % (k,x) for x in xs))+')'
             for k,xs in labels)
    ):
        if first: first = False
        else: sys.stdout.write(',')
        sys.stdout.write(
          '[['+h[0]+'],['+h[1]+'],\"'+
          ' '.join('*' if x=='' else x for x in h[2:])+'\"]')

sys.stdout.write(']')

db.close()

