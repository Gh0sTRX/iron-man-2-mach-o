#!/usr/bin/env python3
"""Coarse symbol-name triage; not proof of runtime usage or implementation."""
import json,sys,collections
p=sys.argv[1]
names=json.load(open(p))['dynamic_imports']
def category(n):
 s=n.lstrip('_')
 if s.startswith(('objc_','OBJC_','class_','protocol_','sel_')) or s in ('NSLog','NSClassFromString','NSStringFromClass'):return 'Objective-C runtime'
 if s.startswith(('gl','egl','EAGL','CAEAGL')):return 'Graphics / OpenGL ES'
 if s.startswith(('al','alc','Audio','ExtAudio','OpenAL','AUGraph','Music','kAudio')):return 'Audio'
 if s.startswith(('CF','CG','CT','UI','NS','UIApplication','UIGraphics','CGBitmap')):return 'Apple frameworks'
 if s.startswith(('pthread','dispatch','mach_','OSAtomic','semaphore','sysctl','gettimeofday','clock_')):return 'Threads / system'
 if s.startswith(('socket','send','recv','connect','bind','listen','accept','getaddr','select','setsockopt','inet_','gethost')):return 'Networking'
 if s.startswith(('fopen','fclose','fread','fwrite','open','close','read','write','stat','lseek','opendir','readdir','remove','rename','mkdir','realpath')):return 'Filesystem / POSIX'
 return 'C library / other'
groups=collections.defaultdict(list)
for n in names:groups[category(n)].append(n)
print(json.dumps({'total':len(names),'warning':'Name-prefix classification only, not runtime coverage','groups':{k:{'count':len(v),'symbols':v} for k,v in sorted(groups.items())}},indent=2))
