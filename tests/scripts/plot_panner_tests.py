import matplotlib.pyplot as plt
import pandas as pd
import os
import glob

def plot_stereo_sweep():
    file_path = 'tests/output/stereo_panner_sweep.csv'
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return

    df = pd.read_csv(file_path)
    
    plt.figure(figsize=(10, 6))
    plt.plot(df['Pan'], df['Left_RMS'], label='Left Channel')
    plt.plot(df['Pan'], df['Right_RMS'], label='Right Channel')
    plt.plot(df['Pan'], df['Total_Power'], label='Total Power', linestyle='--')
    
    plt.title('Stereo Panner Sweep')
    plt.xlabel('Pan Position (0=Left, 1=Right)')
    plt.ylabel('RMS Level')
    plt.grid(True)
    plt.legend()
    
    os.makedirs('tests/plots/panner', exist_ok=True)
    plt.savefig('tests/plots/panner/stereo_sweep.png')
    plt.close()
    print("Generated stereo_sweep.png")

def plot_quad_sweep():
    file_path = 'tests/output/quad_panner_sweep.csv'
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return

    df = pd.read_csv(file_path)
    
    plt.figure(figsize=(12, 8))
    
    # Subplot 1: Gains vs Time
    plt.subplot(2, 1, 1)
    plt.plot(df['Time'], df['FL'], label='Front Left')
    plt.plot(df['Time'], df['FR'], label='Front Right')
    plt.plot(df['Time'], df['BL'], label='Back Left')
    plt.plot(df['Time'], df['BR'], label='Back Right')
    plt.title('Quad Panner Gains (Circular Sweep)')
    plt.xlabel('Step')
    plt.ylabel('RMS Level')
    plt.grid(True)
    plt.legend()

    # Subplot 2: Trajectory (X vs Y)
    plt.subplot(2, 1, 2)
    plt.plot(df['PanX'], df['PanY'], label='Trajectory')
    plt.xlim(0, 1)
    plt.ylim(0, 1)
    plt.title('Panner Trajectory')
    plt.xlabel('Pan X')
    plt.ylabel('Pan Y')
    plt.grid(True)
    plt.gca().set_aspect('equal')
    
    plt.tight_layout()
    plt.savefig('tests/plots/panner/quad_sweep.png')
    plt.close()
    print("Generated quad_sweep.png")

def plot_cleat_sweep():
    file_path = 'tests/output/cleat_panner_sweep.csv'
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return

    df = pd.read_csv(file_path)
    
    plt.figure(figsize=(14, 8))
    
    # Plot all 16 channels
    # Channels are 0-15
    for i in range(16):
        plt.plot(df['Time'], df[f'Ch{i}'], alpha=0.7, label=f'Ch{i}')
        
    plt.title('CLEAT Panner Sweep (Diagonal 0,0 -> 1,1)')
    plt.xlabel('Pan Position')
    plt.ylabel('RMS Level')
    plt.grid(True)
    # Legend might be too big, maybe only show a few or put outside
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left', borderaxespad=0.)
    
    plt.tight_layout()
    plt.savefig('tests/plots/panner/cleat_sweep.png')
    plt.close()
    print("Generated cleat_sweep.png")

if __name__ == "__main__":
    print("Generating panner plots...")
    plot_stereo_sweep()
    plot_quad_sweep()
    plot_cleat_sweep()

