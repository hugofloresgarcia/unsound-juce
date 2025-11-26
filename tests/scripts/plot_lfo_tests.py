import glob
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import wave

# Configuration for specific plots
PLOT_CONFIG = {
    'clocked_mode': {'subplots': True},
    'basic_waveform': {'subplots': True},
    'slop_humanization': {'subplots': True},
    'delay_param': {'subplots': True},
    'level_polarity': {'subplots': True}
}

def get_html_header():
    return """
    <!DOCTYPE html>
    <html>
    <head>
        <title>LFO DSP Test Report</title>
        <style>
            body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 20px; background: #f0f0f0; }
            h1 { text-align: center; color: #333; }
            .container { max-width: 1200px; margin: 0 auto; }
            .card { background: white; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); margin-bottom: 20px; padding: 20px; }
            .card h2 { margin-top: 0; color: #555; border-bottom: 1px solid #eee; padding-bottom: 10px; }
            .plot-img { width: 100%; height: auto; border: 1px solid #eee; margin-bottom: 10px; }
            audio { width: 100%; margin-top: 10px; }
            .grid-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 20px; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>LFO DSP Test Report</h1>
            <div class="grid-container">
    """

def get_html_footer():
    return """
            </div>
        </div>
    </body>
    </html>
    """

def generate_spectrogram(wav_file, plots_dir):
    try:
        with wave.open(wav_file, 'rb') as wf:
            framerate = wf.getframerate()
            nframes = wf.getnframes()
            str_data = wf.readframes(nframes)
            wave_data = np.frombuffer(str_data, dtype=np.int16)
            
            # Normalize
            wave_data = wave_data / 32768.0
            
            plt.figure(figsize=(10, 4))
            plt.specgram(wave_data, NFFT=1024, Fs=framerate, noverlap=512, cmap='inferno')
            plt.ylabel('Frequency (Hz)')
            plt.xlabel('Time (s)')
            plt.title(f'Spectrogram: {os.path.basename(wav_file)}')
            plt.colorbar(format='%+2.0f dB')
            plt.ylim(0, 1000) # Limit freq range since we only care about 220-440Hz modulation
            plt.tight_layout()
            
            base_name = os.path.basename(wav_file).replace('.wav', '')
            out_path = os.path.join(plots_dir, f"{base_name}_spec.png")
            plt.savefig(out_path)
            plt.close()
            print(f"Generated spectrogram: {out_path}")
            return f"{base_name}_spec"
    except Exception as e:
        print(f"Error generating spectrogram for {wav_file}: {e}")
        return None

def generate_width_matrix_plot(output_dir, plots_dir):
    # Find all width CSVs
    width_files = glob.glob(os.path.join(output_dir, 'width_*.csv'))
    if not width_files:
        return None

    # Group by waveform and width
    # format: width_TYPE_WIDTHINT.csv
    data = {}
    widths = set()
    types = set()

    for f in width_files:
        name = os.path.basename(f).replace('.csv', '')
        parts = name.split('_')
        if len(parts) != 3: continue
        w_type = parts[1]
        w_val = int(parts[2])
        
        types.add(w_type)
        widths.add(w_val)
        
        if w_type not in data: data[w_type] = {}
        data[w_type][w_val] = f

    sorted_widths = sorted(list(widths))
    sorted_types = sorted(list(types))

    if not sorted_widths or not sorted_types:
        return None

    fig, axes = plt.subplots(len(sorted_types), len(sorted_widths), figsize=(15, 10), sharex=True, sharey=True)
    fig.suptitle('Width Parameter Matrix', fontsize=16)

    # Ensure axes is 2D array
    if len(sorted_types) == 1: axes = [axes]
    if len(sorted_widths) == 1: axes = [[ax] for ax in axes]

    for i, w_type in enumerate(sorted_types):
        for j, w_val in enumerate(sorted_widths):
            ax = axes[i][j]
            if w_val in data[w_type]:
                df = pd.read_csv(data[w_type][w_val])
                ax.plot(df['Time'], df['Value'])
                ax.set_title(f'{w_type} Width={w_val}%')
                ax.grid(True, alpha=0.3)
                ax.set_ylim(-1.1, 1.1)
            
            if i == len(sorted_types) - 1:
                ax.set_xlabel('Time')
            if j == 0:
                ax.set_ylabel('Value')

    plt.tight_layout()
    out_path = os.path.join(plots_dir, 'width_matrix.png')
    plt.savefig(out_path)
    plt.close()
    print(f"Generated matrix plot: {out_path}")
    return 'width_matrix'

def plot_csv(csv_file, plots_dir):
    try:
        df = pd.read_csv(csv_file)
        base_name = os.path.basename(csv_file).replace('.csv', '')
        
        # Determine generic config
        use_subplots = False
        for key in PLOT_CONFIG:
            if key in base_name:
                use_subplots = PLOT_CONFIG[key].get('subplots', False)
                break
        
        # Identify X axis
        if 'Time' in df.columns:
            x_col = 'Time'; x_label = 'Time (s)'
        elif 'Beat' in df.columns:
            x_col = 'Beat'; x_label = 'Beats'
        elif 'Step' in df.columns:
            x_col = 'Step'; x_label = 'Step'
        else:
            x_col = df.columns[0]; x_label = x_col

        # Identify Y columns
        y_cols = [c for c in df.columns if c not in [x_col, 'Phase', 'Step']]
        
        # Filter out auxiliary step col if x is not step
        if x_col != 'Step':
            y_cols = [c for c in y_cols if c != 'Step']

        num_plots = len(y_cols)
        
        if use_subplots and num_plots > 1:
            fig, axes = plt.subplots(num_plots, 1, figsize=(10, 3 * num_plots), sharex=True)
            if num_plots == 1: axes = [axes]
            
            for i, col in enumerate(y_cols):
                ax = axes[i]
                if 'euclidean' in base_name or 'Square' in base_name or 'Gate' in base_name:
                    ax.step(df[x_col], df[col], where='post', color=f'C{i}')
                else:
                    ax.plot(df[x_col], df[col], color=f'C{i}')
                
                ax.set_ylabel(col)
                ax.grid(True, alpha=0.3)
                ax.set_ylim(-1.1, 1.1)
                ax.axhline(0, color='k', alpha=0.2)
            
            axes[-1].set_xlabel(x_label)
            fig.suptitle(f'LFO Test: {base_name}')
            plt.tight_layout()
        else:
            # Single plot with overlaid lines
            plt.figure(figsize=(10, 6))
            for col in y_cols:
                if 'euclidean' in base_name or 'Square' in base_name or 'Gate' in base_name:
                    plt.step(df[x_col], df[col], where='post', label=col)
                else:
                    plt.plot(df[x_col], df[col], label=col)
            
            plt.axhline(0, color='black', linewidth=0.5, alpha=0.5)
            plt.grid(True, alpha=0.3)
            plt.xlabel(x_label)
            plt.ylabel('Amplitude')
            plt.title(f'LFO Test: {base_name}')
            plt.legend()
            plt.tight_layout()

        output_path = os.path.join(plots_dir, f"{base_name}.png")
        plt.savefig(output_path)
        plt.close()
        print(f"Generated {output_path}")
        return base_name

    except Exception as e:
        print(f"Error plotting {csv_file}: {e}")
        return None

def generate_report(output_dir='tests/output'):
    plots_dir = os.path.join('tests', 'plots')
    os.makedirs(plots_dir, exist_ok=True)
    
    html_content = get_html_header()
    
    # 1. Special Width Matrix
    width_matrix_name = generate_width_matrix_plot(output_dir, plots_dir)
    if width_matrix_name:
         html_content += f"""
            <div class="card" style="grid-column: 1 / -1;">
                <h2>Width Parameter Matrix</h2>
                <img src="../plots/{width_matrix_name}.png" class="plot-img">
                <!-- No single audio for matrix -->
            </div>
        """

    # 2. Process all CSVs
    csv_files = sorted(glob.glob(os.path.join(output_dir, '*.csv')))
    
    # Skip width files as they are handled in matrix
    csv_files = [f for f in csv_files if not os.path.basename(f).startswith('width_')]

    for csv_file in csv_files:
        base_name = plot_csv(csv_file, plots_dir)
        if base_name:
            # Check for audio file
            audio_file = os.path.join(output_dir, base_name + '.wav')
            audio_html = ""
            spectrogram_html = ""
            
            if os.path.exists(audio_file):
                # Generate spectrogram
                spec_name = generate_spectrogram(audio_file, plots_dir)
                if spec_name:
                    spectrogram_html = f'<img src="../plots/{spec_name}.png" class="plot-img" style="margin-top: 10px;">'

                # Browser needs relative path
                audio_rel_path = f"../output/{base_name}.wav"
                audio_html = f'<audio controls><source src="{audio_rel_path}" type="audio/wav">Your browser does not support the audio element.</audio>'
            
            html_content += f"""
            <div class="card">
                <h2>{base_name}</h2>
                <img src="../plots/{base_name}.png" class="plot-img">
                {spectrogram_html}
                {audio_html}
            </div>
            """

    html_content += get_html_footer()
    
    report_path = os.path.join('tests', 'report', 'index.html')
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    
    with open(report_path, 'w') as f:
        f.write(html_content)
    
    print(f"Report generated at: {report_path}")

if __name__ == "__main__":
    generate_report()
