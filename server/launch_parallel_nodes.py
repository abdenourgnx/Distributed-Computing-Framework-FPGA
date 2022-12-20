import sys
from subprocess import Popen

def main(n_nodes: int):
    
    nodes = [Popen("./build/test_node") for _ in range(n_nodes)]

    for node in nodes:
        node.wait(20)

    return



if __name__ == "__main__":

    if len(sys.argv) != 2:
        print("Usage:", sys.argv[0], "[Number of nodes]")
        exit(-1)

    try:
        main(int(sys.argv[1]))
    except:
        print("Invalid argument:", sys.argv[1])
        print("Usage:", sys.argv[0], "[Number of nodes]")
        exit(-2)
    
    