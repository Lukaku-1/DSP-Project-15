clc;
clear;
close all;

%% 1. DEFINE TRANSMITTER PARAMETERS
Fs = 8000;                  % Sampling rate (8 kHz standard telephony rate)
tone_duration = 0.2;        % 200 ms tone duration
pause_duration = 0.1;       % 100 ms silent gap between consecutive digits
space_pause_duration = 0.5; % 500 ms delay when a space ' ' is entered

% Define keypad frequencies (4x3 Grid)
row_freqs = [697, 770, 852, 941]; % Low frequency group (Hz)
col_freqs = [1209, 1336, 1477];   % High frequency group (Hz)

% Map keypad to standard 4x3 matrix layout
keypad = {'1', '2', '3'; 
          '4', '5', '6'; 
          '7', '8', '9';
          '*', '0', '#'};

% Create figure once outside the loop for fast updating
fig = figure('Name', 'DTMF Transmitter Signal', 'NumberTitle', 'off');

%% 2. TRANSMITTER CONTINUOUS LOOP
disp('====================================================');
disp('              DTMF TRANSMITTER SYSTEM               ');
disp('  Enter key sequences using 0-9, *, # (e.g., 123 456) ');
disp('  Use spaces " " to insert extra delay between digits');
disp('  Type "exit" or press Enter on empty line to QUIT   ');
disp('====================================================');

while true
    fprintf('\n');
    sequence = input('Enter sequence: ', 's');
    
    % Check for exit conditions
    if isempty(sequence) || strcmpi(sequence, 'exit') || strcmpi(sequence, 'quit')
        disp('Exiting DTMF Transmitter. Goodbye!');
        break;
    end
    
    %% 3. GENERATE DUAL-TONE AUDIO SIGNAL
    tx_signal = [];
    t_tone = 0:(1/Fs):(tone_duration - 1/Fs);
    t_pause = zeros(1, round(pause_duration * Fs));
    t_space = zeros(1, round(space_pause_duration * Fs));

    for i = 1:length(sequence)
        key = sequence(i);
        
        % Handle SPACE character -> Add extra silent delay
        if strcmp(key, ' ')
            tx_signal = [tx_signal, t_space];
            continue;
        end
        
        % Locate row and column of pressed key
        [row, col] = find(strcmp(keypad, key));
        
        if isempty(row)
            warning('Skipping invalid character: "%s". Only 0-9, *, #, space allowed.', key);
            continue;
        end
        
        % Pick the two frequencies for this key
        f_low = row_freqs(row);
        f_high = col_freqs(col);
        
        % Synthesize low wave, high wave, and sum them together
        low_wave  = sin(2 * pi * f_low * t_tone);
        high_wave = sin(2 * pi * f_high * t_tone);
        dual_tone = low_wave + high_wave;
        
        % Normalize signal to prevent clipping
        dual_tone = dual_tone / max(abs(dual_tone));
        
        % Combine tone and silent pause into main signal
        tx_signal = [tx_signal, dual_tone, t_pause];
    end

    % Check if valid audio was generated
    if isempty(tx_signal)
        disp('No valid DTMF characters or spaces were processed.');
        continue;
    end

    %% 4. UPDATE WAVEFORM & SPECTROGRAM PLOTS
    if ishandle(fig)
        figure(fig); % Bring figure to focus
        
        t_axis = (0:length(tx_signal)-1) / Fs;

        % Subplot 1: Time-Domain Waveform
        subplot(2,1,1);
        plot(t_axis, tx_signal, 'b');
        title(['Time-Domain Waveform for Sequence: "', sequence, '"']);
        xlabel('Time (seconds)');
        ylabel('Amplitude');
        ylim([-1.2 1.2]);
        grid on;

        % Subplot 2: Spectrogram
        subplot(2,1,2);
        spectrogram(tx_signal, 256, 200, 512, Fs, 'yaxis');
        title('Frequency Spectrogram (Dual-Tone Pairs)');
        ylim([0 2.0]); % Show 0 kHz to 2 kHz range
        drawnow;
    end

    %% 5. PLAY TRANSMITTED SOUND AND WAIT
    fprintf('Playing DTMF audio for "%s"...\n', sequence);
    
    % Save to file as a backup
    audiowrite('dtmf_output.wav', tx_signal, Fs);

    % Play audio with blocking wait so it finishes before next input prompt
    try
        player = audioplayer(tx_signal, Fs);
        playblocking(player); % Blocks execution until audio finishes playing
    catch
        try
            sound(tx_signal, Fs);
            pause(length(tx_signal)/Fs); % Pause loop while sound plays
        catch
            disp('Audio hardware error. Audio saved to "dtmf_output.wav".');
        end
    end
end
