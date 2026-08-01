using SmoothEverything.Settings.Models;
using System.Diagnostics;
using System.IO.Pipes;
using System.Text;
using System.Text.Json;

namespace SmoothEverything.Settings.Services;

public sealed class SettingsSession
{
    private const string PipeName = "SmoothEverything.Engine.v1";
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        WriteIndented = true,
    };

    private readonly SemaphoreSlim _operationGate = new(1, 1);
    private readonly object _debounceLock = new();
    private CancellationTokenSource? _debounceCancellation;
    private bool _initialized;

    public static SettingsSession Instance { get; } = new();

    public event EventHandler? StateChanged;

    public AppSettingsModel Current { get; private set; } = new();
    public EngineDiagnosticsModel Diagnostics { get; private set; } = new();
    public bool IsOnline { get; private set; }
    public string StatusText { get; private set; } = "正在连接引擎…";
    public string LastError { get; private set; } = string.Empty;
    public string SettingsFilePath { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SmoothEverything",
        "settings.json");

    private SettingsSession()
    {
    }

    public async Task InitializeAsync()
    {
        if (_initialized)
        {
            return;
        }
        _initialized = true;

        if (await TryRefreshAsync(replaceSettings: true).ConfigureAwait(true))
        {
            return;
        }

        if (TryStartSiblingEngine())
        {
            for (var attempt = 0; attempt < 20; ++attempt)
            {
                await Task.Delay(100).ConfigureAwait(true);
                if (await TryRefreshAsync(replaceSettings: true).ConfigureAwait(true))
                {
                    return;
                }
            }
        }

        await LoadLocalAsync().ConfigureAwait(true);
        IsOnline = false;
        StatusText = "引擎离线；更改仍会保存到本机";
        RaiseStateChanged();
    }

    public async Task RefreshAsync()
    {
        if (!await TryRefreshAsync(replaceSettings: true).ConfigureAwait(true))
        {
            IsOnline = false;
            StatusText = "未连接到引擎";
            RaiseStateChanged();
        }
    }

    public async Task EnsureEngineAsync()
    {
        if (await TryRefreshAsync(replaceSettings: true).ConfigureAwait(true))
        {
            return;
        }

        if (!TryStartSiblingEngine())
        {
            IsOnline = false;
            LastError = "在设置程序同目录中找不到 SmoothEverything.Engine.exe";
            StatusText = "无法启动引擎";
            RaiseStateChanged();
            return;
        }

        for (var attempt = 0; attempt < 30; ++attempt)
        {
            await Task.Delay(100).ConfigureAwait(true);
            if (await TryRefreshAsync(replaceSettings: true).ConfigureAwait(true))
            {
                return;
            }
        }

        IsOnline = false;
        LastError = "引擎启动后未能建立命名管道连接";
        StatusText = "引擎连接超时";
        RaiseStateChanged();
    }

    public void QueueApply()
    {
        CancellationTokenSource cancellation;
        lock (_debounceLock)
        {
            _debounceCancellation?.Cancel();
            _debounceCancellation?.Dispose();
            _debounceCancellation = new CancellationTokenSource();
            cancellation = _debounceCancellation;
        }

        StatusText = "正在保存更改…";
        RaiseStateChanged();
        _ = ApplyAfterDelayAsync(cancellation.Token);
    }

    public async Task ApplyNowAsync()
    {
        await _operationGate.WaitAsync().ConfigureAwait(true);
        try
        {
            Current.Normalize();
            var response = await TryRequestAsync(new { op = "apply_settings", settings = Current })
                .ConfigureAwait(true);
            if (response is { Ok: true })
            {
                IsOnline = true;
                Diagnostics = response.Diagnostics ?? Diagnostics;
                LastError = response.Error;
                StatusText = "已应用到引擎";
                RaiseStateChanged();
                return;
            }

            await SaveLocalAtomicAsync().ConfigureAwait(true);
            IsOnline = false;
            LastError = response?.Error ?? LastError;
            StatusText = "已保存；引擎离线";
            RaiseStateChanged();
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            IsOnline = false;
            LastError = error.Message;
            StatusText = "保存失败";
            RaiseStateChanged();
        }
        finally
        {
            _operationGate.Release();
        }
    }

    private async Task ApplyAfterDelayAsync(CancellationToken cancellationToken)
    {
        try
        {
            await Task.Delay(250, cancellationToken).ConfigureAwait(true);
            await ApplyNowAsync().ConfigureAwait(true);
        }
        catch (OperationCanceledException)
        {
        }
    }

    private async Task<bool> TryRefreshAsync(bool replaceSettings)
    {
        var response = await TryRequestAsync(new { op = "get_state" }).ConfigureAwait(true);
        if (response is not { Ok: true })
        {
            return false;
        }

        if (replaceSettings && response.Settings is not null)
        {
            response.Settings.Normalize();
            Current = response.Settings;
        }
        Diagnostics = response.Diagnostics ?? Diagnostics;
        IsOnline = true;
        LastError = response.Error;
        StatusText = "引擎已连接";
        RaiseStateChanged();
        return true;
    }

    private async Task<EngineResponseModel?> TryRequestAsync<TRequest>(TRequest request)
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromMilliseconds(1200));
        try
        {
            await using var pipe = new NamedPipeClientStream(
                ".",
                PipeName,
                PipeDirection.InOut,
                PipeOptions.Asynchronous);
            await pipe.ConnectAsync(timeout.Token).ConfigureAwait(true);

            using var writer = new StreamWriter(pipe, new UTF8Encoding(false), 4096, leaveOpen: true)
            {
                AutoFlush = true,
            };
            using var reader = new StreamReader(
                pipe,
                new UTF8Encoding(false),
                detectEncodingFromByteOrderMarks: false,
                bufferSize: 4096,
                leaveOpen: true);
            await writer.WriteLineAsync(JsonSerializer.Serialize(request, JsonOptions).AsMemory(), timeout.Token)
                .ConfigureAwait(true);
            var line = await reader.ReadLineAsync(timeout.Token).ConfigureAwait(true);
            if (string.IsNullOrWhiteSpace(line))
            {
                return null;
            }
            return JsonSerializer.Deserialize<EngineResponseModel>(line, JsonOptions);
        }
        catch (Exception error) when (error is IOException or TimeoutException or OperationCanceledException)
        {
            LastError = error is OperationCanceledException ? "连接引擎超时" : error.Message;
            return null;
        }
        catch (JsonException error)
        {
            LastError = $"引擎返回了无效数据：{error.Message}";
            return null;
        }
    }

    private async Task LoadLocalAsync()
    {
        try
        {
            if (!File.Exists(SettingsFilePath))
            {
                Current = new AppSettingsModel();
                return;
            }
            await using var stream = File.OpenRead(SettingsFilePath);
            var parsed = await JsonSerializer.DeserializeAsync<AppSettingsModel>(stream, JsonOptions)
                .ConfigureAwait(true);
            if (parsed is not null && parsed.SchemaVersion == 1)
            {
                parsed.Normalize();
                Current = parsed;
            }
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException)
        {
            LastError = $"无法读取本地配置：{error.Message}";
            Current = new AppSettingsModel();
        }
    }

    private async Task SaveLocalAtomicAsync()
    {
        var directory = Path.GetDirectoryName(SettingsFilePath)!;
        Directory.CreateDirectory(directory);
        var temporaryPath = SettingsFilePath + ".settings.tmp";
        var json = JsonSerializer.Serialize(Current, JsonOptions);
        await using (var stream = new FileStream(
            temporaryPath,
            FileMode.Create,
            FileAccess.Write,
            FileShare.None,
            16 * 1024,
            FileOptions.Asynchronous | FileOptions.WriteThrough))
        {
            var bytes = Encoding.UTF8.GetBytes(json);
            await stream.WriteAsync(bytes).ConfigureAwait(true);
            await stream.FlushAsync().ConfigureAwait(true);
            stream.Flush(flushToDisk: true);
        }
        File.Move(temporaryPath, SettingsFilePath, overwrite: true);
    }

    private static bool TryStartSiblingEngine()
    {
        try
        {
            var enginePath = Path.Combine(AppContext.BaseDirectory, "SmoothEverything.Engine.exe");
            if (!File.Exists(enginePath))
            {
                return false;
            }
            Process.Start(new ProcessStartInfo
            {
                FileName = enginePath,
                UseShellExecute = false,
                CreateNoWindow = true,
                WorkingDirectory = AppContext.BaseDirectory,
            });
            return true;
        }
        catch (Exception error) when (error is InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            return false;
        }
    }

    private void RaiseStateChanged() => StateChanged?.Invoke(this, EventArgs.Empty);
}
