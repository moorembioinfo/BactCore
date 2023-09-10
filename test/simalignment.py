
import random
import argparse

def generate_random_sequence(length):
  return ''.join(random.choice('ACGT') for _ in range(length))

def introduce_columnwise_gaps(sequences, precalc_positions, threshold_high):
  for col in precalc_positions:
    num_artifacts = random.randint(int(len(sequences) * threshold_high) + 1, len(sequences))
    for pos in range(num_artifacts):
      sequences[pos][col] = '-'

def generate_and_write_sequences(num_sequences, seq_length, precalc_positions, threshold_high, batch_size, filename):
  with open(filename, "w") as f:
    for batch_start in range(0, num_sequences, batch_size):
      batch_end = min(batch_start + batch_size, num_sequences)
      first_sequence = generate_random_sequence(seq_length)
      sequences = [first_sequence for _ in range(batch_end - batch_start)]
      sequences = [list(seq) for seq in sequences]
      introduce_columnwise_gaps(sequences, precalc_positions, threshold_high)
      for i, seq in enumerate(sequences):
        f.write(f">seq_{batch_start + i}\n{''.join(seq)}\n")

parser = argparse.ArgumentParser(description="Simulate alignment with gaps.")
parser.add_argument("-n", "--num_sequences", type=int, default=100, help="Number of sequences to generate.")
parser.add_argument("-b", "--batch_size", type=int, default=10, help="Number of sequences to generate in each batch.")
args = parser.parse_args()
num_sequences = args.num_sequences
batch_size = args.batch_size

threshold_high = 0.05
seq_length = 5000000
precalc_positions = random.sample(range(seq_length), 500000)
generate_and_write_sequences(num_sequences, seq_length, precalc_positions, threshold_high, batch_size, "alignment.fasta")
