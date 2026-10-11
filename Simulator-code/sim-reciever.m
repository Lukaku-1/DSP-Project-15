clear; clc; close all;

%% 1. DTMF Setup & Key Generation
key = '9';
Fs = 8000; N = 4000; t = (0:N-1)/Fs;
row_f = [697, 770, 852, 941];
col_f = [1209, 1336, 1477, 1633];
pad   = ['1','2','3','A'; 
    '4','5','6','B'; 
    '7','8','9','C'; 
    '*','0','#','D'];

[r, c] = find(pad == key);
clean = 0.5*sin(2*pi*row_f(r)*t) + 0.5*sin(2*pi*col_f(c)*t);

%% 2. Add Noise & Unwanted Tones (Channel)
noisy = clean + 0.2*sin(2*pi*350*t) + 0.15*sin(2*pi*2100*t) + 0.2*randn(size(t));

%% 3. FIR Bandpass Filter (650 Hz - 1700 Hz)
% Order 100 FIR filter isolating the standard DTMF frequency range
filter_order = 100;
f_pass = [650, 1700] / (Fs/2); % Normalized cutoffs (0 to 1, where 1 is Nyquist)
b_fir = fir1(filter_order, f_pass, 'bandpass');

% filtfilt applies zero-phase filtering to prevent phase/frequency shifts
filtered = filtfilt(b_fir, 1, noisy);

%% 4. FFT Detection (Performed on Filtered Signal)
f_axis = (0:N/2)*(Fs/N);

% Frequency analysis
clean_mag    = abs(fft(clean));    clean_mag    = clean_mag(1:N/2+1);
noisy_mag    = abs(fft(noisy));    noisy_mag    = noisy_mag(1:N/2+1);
filtered_mag = abs(fft(filtered)); filtered_mag = filtered_mag(1:N/2+1);

% Detect dominant peak in low and high groups
[~, idx_low]  = max(filtered_mag .* (f_axis >= 650 & f_axis <= 1000));
[~, idx_high] = max(filtered_mag .* (f_axis >= 1150 & f_axis <= 1700));

[~, r_fft] = min(abs(row_f - f_axis(idx_low)));
[~, c_fft] = min(abs(col_f - f_axis(idx_high)));
key_fft = pad(r_fft, c_fft);

%% 5. Goertzel Detection (8 DTMF Bins on Filtered Signal)
dtmf_f = [row_f, col_f];
goertzel_mag = zeros(1, 8);

for k = 1:8
    coeff = 2*cos(2*pi*dtmf_f(k)/Fs);
    s = 0; s1 = 0; s2 = 0;
    for x = filtered
        s = x + coeff*s1 - s2;
        s2 = s1; s1 = s;
    end
    goertzel_mag(k) = s1^2 + s2^2 - coeff*s1*s2;
end

[~, r_gz] = max(goertzel_mag(1:4));
[~, c_gz] = max(goertzel_mag(5:8));
key_gz = pad(r_gz, c_gz);

%% 6. Display & Multi-Stage Plots
fprintf('Key: %c | FFT: %c (%.0f, %.0f Hz) | Goertzel: %c (%d, %d Hz)\n', ...
    key, key_fft, f_axis(idx_low), f_axis(idx_high), key_gz, row_f(r_gz), col_f(c_gz));

figure('Color', [1 1 1], 'Position', [100 100 800 700]);

% Stage 1: Clean Input Spectrum
subplot(4,1,1); 
plot(f_axis, clean_mag, 'g', 'LineWidth', 1.2); 
xlim([0 2500]); grid on;
title(sprintf('1. Clean Spectrum (Key %c: %d & %d Hz)', key, row_f(r), col_f(c)));
xlabel('Frequency (Hz)'); ylabel('Magnitude');

% Stage 2: Noisy Spectrum before filtering
subplot(4,1,2); 
plot(f_axis, noisy_mag, 'Color', [0.85 0.33 0.1]); 
xlim([0 2500]); grid on;
title('2. Noisy Spectrum (With 350 Hz, 2100 Hz & Air Noise)');
xlabel('Frequency (Hz)'); ylabel('Magnitude');

% Stage 3: Filtered Spectrum & FFT Detection
subplot(4,1,3); 
plot(f_axis, filtered_mag, 'b', 'LineWidth', 1.1); hold on; grid on;
xline(f_axis(idx_low), '--r', sprintf('%.0f Hz', f_axis(idx_low)), 'LabelVerticalAlignment', 'bottom'); 
xline(f_axis(idx_high), '--r', sprintf('%.0f Hz', f_axis(idx_high)), 'LabelVerticalAlignment', 'bottom'); 
xlim([500 1800]);
title(sprintf('3. FIR Filtered Spectrum & FFT Detection (Decoded: %c)', key_fft));
xlabel('Frequency (Hz)'); ylabel('Magnitude');

% Stage 4: Goertzel Output on Filtered Signal
subplot(4,1,4); 
stem(dtmf_f, goertzel_mag, 'm', 'filled', 'LineWidth', 1.2); 
grid on; xticks(dtmf_f);
title(sprintf('4. Goertzel Output (8 Target Bins | Decoded: %c)', key_gz));
xlabel('Frequency (Hz)'); ylabel('Power');
