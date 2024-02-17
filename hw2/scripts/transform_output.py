import argparse
import re

# change random pid and secret into [serivce_name-{pid,secret}]
def transform_output(output_lines):
    secret_to_name = dict()
    trans_lines = []
    for line in output_lines:
        if "has been spawned" in line:
            match = re.search("pid: (?P<pid>\d+), secret: (?P<secret>\d+)", line)
            service_name, pid, secret = line.split()[0], match.group("pid"), match.group("secret")
            secret_to_name[secret] = service_name
            line = line.replace(secret, f"[{service_name}-secret]") \
                        .replace(pid, f"[{service_name}-pid]")
        elif "new secret" in line:
            secret = line.split()[-1]
            line = line.replace(secret, f"[{secret_to_name[secret]}-secret]")
        trans_lines.append(line)
    return trans_lines

def print_transformed_lines(args):
    output_name = args.output
    with open(output_name, "r") as f_output:
        f_output = open(output_name, "r")
        trans_lines = transform_output(f_output.readlines())
    for line in trans_lines:
        print(line, end="")

parser = argparse.ArgumentParser()
parser.add_argument("output", help = "Your file name of output")
args = parser.parse_args()
print_transformed_lines(args)