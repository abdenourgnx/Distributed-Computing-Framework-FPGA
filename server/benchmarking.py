from subprocess import Popen
from subprocess import PIPE
import time
    


def benchmark(n_nodes, payload_size, input_count):

    server = Popen(["./build/server"], stdout=PIPE, stderr=PIPE)

    time.sleep(0.5)

    nodes = [Popen(["./build/test_node"], stdout=PIPE, stderr=PIPE) for _ in range(n_nodes)]

    time.sleep(0.5)

    user = Popen(["./build/test_user", str(payload_size), str(input_count), str(0)], stdout=PIPE, stderr=PIPE, text=True)

    try:
        user.wait(10)
    except:
        print("Failed: ya rebaaaak")

        server.kill()
        for node in nodes:
            node.kill()

        server.wait()
        for node in nodes:
            node.wait()

        return None

    server.kill()
    for node in nodes:
        node.kill()

    server.wait(5)
    for node in nodes:
        node.wait(5)



    lines = user.stdout.readlines()

    lines = [line[:-1] for line in lines]

    if  "Start sending loop" not in lines or \
        "Start receiving loop" not in lines or \
        "Stopping sending loop" not in lines or \
        "Stopping receiving loop" not in lines:

        print("Failed: Debugging lines not found")
        return None

    sending_throughput = -1.0
    receiving_throughput = -1.0
    latency_avg = -1.0

    for line in lines:

        if line.startswith("sending_throughput= "):
            try:
                sending_throughput = float(line.split(" ")[1])
            except:
                pass

        if line.startswith("receiving_throughput= "):
            try:
                receiving_throughput = float(line.split(" ")[1])
            except:
                pass

        if line.startswith("latency_avg= "):
            try:
                latency_avg = float(line.split(" ")[1])
            except:
                pass

    if  sending_throughput == -1.0 or \
        receiving_throughput == -1.0 or \
        latency_avg == -1.0:

        print("Didnt receive all data")
        return None

    return sending_throughput, receiving_throughput, latency_avg



results_all = []

def main():

    n_nodes = 1

    payload_size = 16

    input_count = 1

    while n_nodes <= 4:

        results_node = []

        while payload_size <= 131072:

            results_payload = []

            while input_count <= 16384:

                retry = True

                while retry:

                    print("Benchmarking with: n_nodes= " + str(n_nodes) + " | payload_size= " + str(payload_size) + " | input_count= " + str(input_count))
                    
                    results = benchmark(n_nodes, payload_size, input_count)

                    if results:
                        tx_avg, rx_avg, lat_avg = results

                        print("\t Tx avg: " + str(tx_avg))
                        print("\t Rx avg: " + str(rx_avg))
                        print("\t Lat avg: " + str(lat_avg))

                        results_payload.append((tx_avg, rx_avg, lat_avg))
                        retry = False

                    print()

                input_count *= 2

            results_node.append(results_payload)
            print()
            print("Results payload:")
            print(results_payload)
            print()

            payload_size *= 2
            input_count = 1

        results_all.append(results_node)
        print()
        print("Results node:")
        print(results_node)
        print()

        n_nodes += 1

        payload_size = 16
        input_count = 1

    with open("benchmark_stack.data", "w") as f:
        f.write(str(results_all))

    print()
    print("Results all:")
    print(results_all)
    print()

if __name__ == "__main__":
    main()