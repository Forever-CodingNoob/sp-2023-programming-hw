import argparse
import re
import signal
import time
import subprocess
from subprocess import Popen, PIPE
import time

"""
The script only work under Unix environemnt, such as the workstation. This
script checks the number of the fds of each service. You should compile
service.c into `service` with command `make` before running the script. Notice
that if the order of your output is not correct, the script may not work as
expected. This is because the script relies on the output to determine your
process's state.
"""

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

service_tree  = {"Manager": []}
name_to_info = dict() # info = (pid, num)

def get_service_pid(output_line):
    match = re.search("(?P<service_name>[a-zA-Z]+) has been spawned, pid: (?P<pid>\d+)", output_line)
    service_name, pid = match.group("service_name"), match.group("pid")
    name_to_info[service_name] = [pid, (3, 5)[service_name != "Manager"]]

def kill_service(service_name):
    children = service_tree[service_name]
    for child in children:
        kill_service(child)
    name_to_info.pop(service_name)
    service_tree.pop(service_name)

def handle_input_line(input_line):
    if "kill" in input_line:
        target_name = input_line.split()[1]
        if not target_name in name_to_info:
            return
        kill_service(target_name)
        for (service_name, children) in service_tree.items():
            if target_name in children:
                name_to_info[service_name][1] -= 2
                service_tree[service_name].remove(target_name)
                break
    elif "spawn" in input_line:
        parent_name, child_name = input_line.split()[1], input_line.split()[2]
        if not parent_name in name_to_info:
            return
        name_to_info[parent_name][1] += 2
        service_tree[parent_name].append(child_name)
        service_tree[child_name] = []


def handle_output_line(output_line):
    if "has been spawned" in output_line:
        get_service_pid(output_line)

def check_fd_num():
    is_correct = True
    for service_name, (pid, fd_num) in name_to_info.items():
        ls = Popen(["ls", f"/proc/{pid}/fd"], stdout = PIPE)
        ls_output = ls.communicate()[0].decode("ascii").split()
        fd_num_ls = len(ls_output)
        if fd_num_ls > fd_num:
            print(bcolors.FAIL + f"{service_name} has too many file descriptors!"
                  f"[ans, yours] = {[fd_num, fd_num_ls]}" + bcolors.ENDC)
            is_correct = False
        if fd_num_ls < fd_num:
            print(bcolors.OKGREEN + f"{service_name} has less then {fd_num} file descriptors?"
                  f"How do you do that? :O [ans, yours] = {[fd_num, fd_num_ls]}" + bcolors.ENDC)
    if is_correct:
        print(bcolors.OKCYAN + "The number of the fds is correct after the command" + bcolors.ENDC)

def run_service(args):
    input_name, is_ordered = args.input, args.is_ordered == "1"
    manager = Popen(['./service', 'Manager'], bufsize = 0, stdout = PIPE, stdin = PIPE)
    get_service_pid(manager.stdout.readline().decode("ascii"))
    cmd_to_delim = {
        "spawn": "has spawned",
        "kill": "are killed",
        "exchange": "have exchanged",
        "non": "exist",
    }
    with open(input_name, "r") as f_input:
        input_lines = f_input.readlines()
        for input_line in input_lines:
            print(bcolors.HEADER + f"Execute command {input_line[:-1]}" + bcolors.ENDC)
            manager.stdin.write(input_line.encode("ascii"))
            #time.sleep(10)
            if is_ordered:
                target = input_line.split()[1]
                if target not in service_tree:
                    delim = cmd_to_delim["non"]
                else:
                    delim = cmd_to_delim[input_line.split()[0]]
                while True:
                    output_line = manager.stdout.readline().decode("ascii")
                    if delim != cmd_to_delim["non"]:
                        handle_output_line(output_line)
                    if delim in output_line:
                        break
            else:
                try:
                    while True:
                        signal.signal(signal.SIGALRM, lambda : None)
                        signal.setitimer(signal.ITIMER_REAL, 0.01, 0.01)
                        output_line = manager.stdout.readline().decode("ascii")
                        if not output_line:
                            break
                        handle_output_line(output_line)
                except:
                    pass
                signal.setitimer(signal.ITIMER_REAL, 0, 0)
            handle_input_line(input_line)
            check_fd_num()
    subprocess.run(["pkill", "service"])

parser = argparse.ArgumentParser()
parser.add_argument("input", help = "The file name of a command list for ./service")
parser.add_argument("is_ordered",
                    help = "Is the output messages ordered correctly? value: 0/1)",
                    choices = ["0", "1"])
args = parser.parse_args()
run_service(args)
