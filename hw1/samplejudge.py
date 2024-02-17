from argparse import ArgumentParser
import subprocess
import socket
import shutil
import fcntl
import time
import sys
import os

source = ["Makefile", "server.c", "client.c", "hw1.h"]
executable = ["server", "client"]
testpath = "sampletestcases"
timeout = 0.2
parser = ArgumentParser()
parser.add_argument("-t", "--task", choices=["0_1", "0_2", "1_1", "1_2"], nargs="+")
args = parser.parse_args()

class Checker():
    def __init__(self):
        self.score = 0
        self.punishment = 0
        self.fullscore = sum(scores)
        self.io = sys.stderr

    def file_miss(self, files):
        return len(set(files) - set(os.listdir(".")))

    def remove_file(self, files):
        for f in files:
            os.system(f"rm -rf {f}")

    def compile(self):
        ret = os.system("make 1>/dev/null 2>/dev/null")
        if ret != 0 or self.file_miss(executable):
            return False
        return True
        
    def makeclean(self):
        ret = os.system("make clean 1>/dev/null 2>/dev/null")
        if ret != 0 or self.file_miss(executable)!=len(executable):
            self.remove_file(executable)
            return False
        return True

    def run(self):
        print("Checking file format ...", file=self.io)
        if self.file_miss(source):
            print("File not found", file=self.io)
            exit()

        if self.file_miss(executable) != len(executable):
            self.punishment += 0.25
            red("Find executable files", self.io)
            self.remove_file(executable)

        print("Compiling source code ...", file=self.io)
        if not self.compile():
            print("Compiled Error", file=self.io)
            exit()

        print("Testing make clean command ...", file=self.io)
        if not self.makeclean():
            self.punishment += 0.25
            print("Make clean failed", file=self.io)

        self.compile()
        for t, s in zip(testcases, scores):
            self.remove_file(["./BulletinBoard"])
            if t(self.io):
                self.score += s
        
        self.remove_file(executable)

        self.score = max(0, self.score-self.punishment)
        print(f"Final score: {self.score} / {self.fullscore}", file=self.io)

class Server():
    def __init__(self, port):
        self.log = ""
        self.p = subprocess.Popen(["./server", str(port)], stderr=subprocess.DEVNULL, stdout=subprocess.PIPE)
        time.sleep(timeout)

    def exit(self):
        try:
            self.p.terminate()
            self.log = self.p.communicate(timeout=timeout)[0].decode()
            return self.log
        except Exception as e:
            self.p.kill()
            self.p.wait()
            raise Exception(f"Server exit {e}")

class Client():
    def __init__(self, port):
        self.log = ""
        self.p = subprocess.Popen(["./client", "127.0.0.1", str(port)], stdin=subprocess.PIPE, stderr=subprocess.DEVNULL, stdout=subprocess.PIPE)
        time.sleep(timeout)

    def inputfile(self, filename):
        inputs = open(filename, "r")
        while True:
            try:
                res = inputs.readline()
                if res == "pull\n":
                    self.pull()
                if res == "post\n":
                    fr = inputs.readline()
                    co = inputs.readline()
                    self.post(fr, co)
                if res == "exit\n":
                    return self.exit()
            except Exception as e:
                self.p.kill()
                self.p.wait()
                raise e
                
    def pull(self):
        try:
            self.p.stdin.write(b"pull\n")
            self.p.stdin.flush()
            time.sleep(timeout)
        except Exception as e:
            raise Exception(f"Client pull {e}")
    
    def post(self, f, c):
        try:
            self.p.stdin.write(f"post\n{f}{c}".encode())
            self.p.stdin.flush()
            time.sleep(timeout)
        except Exception as e:
            raise Exception(f"Client post {e}")
    
    def exit(self):
        try:
            self.log = self.p.communicate(input="exit\n".encode(), timeout=timeout)[0].decode()
            return self.log
        except Exception as e:
            self.p.kill()
            self.p.wait()
            raise Exception(f"Client exit {e}")

def testcase0_1(io):
    try:
        bold("===== Testcase 0-1 =====", io)
        time.sleep(timeout)
        shutil.copy2(f"{testpath}/testcase0-1-init", "./BulletinBoard")
        port = find_empty_port()
        server = Server(port)
        client = Client(port)
        serverlog, clientlog = "", ""
        clientlog = client.inputfile(f"{testpath}/testcase0-1-input")
        serverlog = server.exit()
        assert compare("./BulletinBoard", f"{testpath}/testcase0-1-record")
        assert strcompare(serverlog, f"{testpath}/testcase0-1-server")
        assert strcompare(clientlog, f"{testpath}/testcase0-1-client")
        green("Testcase 0-1: passed", io)
        return True
    except Exception as e:
        red("Testcase 0-1: failed", io)
        return False

def testcase0_2(io):
    try:
        bold("===== Testcase 0-2 =====", io)
        time.sleep(timeout)
        shutil.copy2(f"{testpath}/testcase0-2-init", "./BulletinBoard")
        port = find_empty_port()
        server = Server(port)
        client = Client(port)
        serverlog, clientlog = "", ""
        clientlog = client.inputfile(f"{testpath}/testcase0-2-input")
        serverlog = server.exit()
        assert strcompare(serverlog, f"{testpath}/testcase0-2-server")
        assert strcompare(clientlog, f"{testpath}/testcase0-2-client")
        assert compare("./BulletinBoard", f"{testpath}/testcase0-2-init")
        green("Testcase 0-2: passed", io)
        return True
    except Exception as e:
        red("Testcase 0-2: failed", io)
        return False

def testcase1_1(io):
    try:
        bold("===== Testcase 1-1 =====", io)
        time.sleep(timeout)
        shutil.copy2(f"{testpath}/testcase1-1-init", "./BulletinBoard")
        port = find_empty_port()
        server = Server(port)
        client = Client(port)
        serverlog, clientlog = "", ""
        clientlog += client.inputfile(f"{testpath}/testcase1-1-input1")
        if not compare("./BulletinBoard", f"{testpath}/testcase1-1-record1"):
            serverlog = server.exit()
            assert False
        client = Client(port)
        clientlog += client.inputfile(f"{testpath}/testcase1-1-input2")
        serverlog = server.exit()
        assert compare("./BulletinBoard", f"{testpath}/testcase1-1-record2")
        assert strcompare(serverlog, f"{testpath}/testcase1-1-server")
        assert strcompare(clientlog, f"{testpath}/testcase1-1-client")
        green("Testcase 1-1: passed", io)
        return True
    except Exception as e:
        red("Testcase 1-1: failed", io)
        return False

def testcase1_2(io):
    try:
        bold("===== Testcase 1-2 =====", io)
        time.sleep(timeout)
        shutil.copy2(f"{testpath}/testcase1-2-init", "./BulletinBoard")
        port = find_empty_port()
        server = Server(port)
        client = Client(port)
        client.pull()
        f = open("./BulletinBoard", "r+")
        record_lock(f, [0])
        client.pull()
        record_unlock(f, [0])
        record_lock(f, [0,1,3,8])
        client.pull()
        record_unlock(f, [0,1,3,8])
        record_lock(f, [i for i in range(10)])
        client.pull()
        record_unlock(f, [i for i in range(10)])
        client.pull()
        clientlog = client.exit()
        serverlog = server.exit()
        assert strcompare(serverlog, f"{testpath}/testcase1-2-server")
        assert strcompare(clientlog, f"{testpath}/testcase1-2-client")
        assert compare("./BulletinBoard", f"{testpath}/testcase1-2-init")
        green("Testcase 1-2: passed", io)
        return True
    except Exception as e:
        red("Testcase 1-2: failed", io)
        return False

def read_record(filename="./BulletinBoard"):
    rec = []
    with open(filename, "rb") as f:
        content = f.read()
        for i in range(0, len(content), 25):
            f = ""
            for ch in content[i:i+5]:
                if ch == 0:
                    break
                f += chr(ch)
            c = ""
            for ch in content[i+5:i+25]:
                if ch == 0:
                    break
                c += chr(ch)
            rec.append({"FROM":f, "CONTENT":c})
        for _ in range(len(rec), 10):
            rec.append({"FROM":"", "CONTENT":""})
    return rec

def record_lock(f, idx):
    for i in idx:
        fcntl.lockf(f, fcntl.LOCK_EX|fcntl.LOCK_NB, 25, 25*i, 0)
    time.sleep(timeout)

def record_unlock(f, idx):
    for i in idx:
        fcntl.lockf(f, fcntl.LOCK_UN, 25, 25*i, 0)
    time.sleep(timeout)

def compare(A, B):
    a = read_record(A)
    b = read_record(B)
    for i, j in zip(a, b):
        if i["FROM"] != j["FROM"]:
            return False
        if i["CONTENT"] != j["CONTENT"]:
            return False
    return True

def strcompare(contentA, B):
    with open(B, "r") as b:
        contentB = b.read()
    return contentA == contentB

def bold(str_, io):
    print("\33[1m" + str_ + "\33[0m", file=io)

def red(str_, io):
    print("\33[31m" + str_ + "\33[0m", file=io)

def green(str_, io):
    print("\33[32m" + str_ + "\33[0m", file=io)

def find_empty_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('localhost', 0))
    _, port = s.getsockname()
    s.close()
    return port

if __name__ == "__main__":
    testcases = [testcase0_1, testcase0_2, testcase1_1, testcase1_2]
    scores = [0.2, 0.2, 0.3, 0.3]
    index = {"0_1":0, "0_2":1, "1_1":2, "1_2":3}
    if args.task is not None:
        task = []
        for t in args.task:
            task.append(index[t])
        task.sort()
        testcases = [testcases[i] for i in task]
        scores = [scores[i] for i in task]
    Checker().run()
