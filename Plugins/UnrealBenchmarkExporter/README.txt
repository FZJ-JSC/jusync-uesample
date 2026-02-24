Unreal Benchmark Exporter Plugin
==================================

A plugin for Unreal Engine that exports benchmark metrics (FPS, memory, CPU usage, vertices, etc.) to Prometheus for visualization in Grafana dashboards.

Features
--------
- Collects real-time performance metrics from Unreal Engine
- Exposes metrics via HTTP endpoint in Prometheus format
- Supports both Windows and Linux platforms
- Easy integration with existing projects
- Blueprint-friendly API

Metrics Collected
-----------------
- FPS (frames per second)
- Frame time (milliseconds)
- Physical memory usage (MB)
- Virtual memory usage (MB)
- CPU usage percentage
- Total vertices rendered
- Total triangles rendered
- Draw calls count
- Rendered primitives count
- GPU memory usage (MB)

Installation
------------
1. Copy the `UnrealBenchmarkExporter` folder to your project's `Plugins` directory
2. Enable the plugin in Edit > Plugins > Utilities > Unreal Benchmark Exporter
3. Restart the editor if required

Usage
-----
The plugin automatically starts when your project loads. It will:
1. Begin collecting metrics immediately (every second)
2. Provide metrics via Blueprint functions

Access metrics via Blueprint:
- Use `Get Benchmark Metrics` to get structured data
- Use `Get Prometheus Metrics Text` to get Prometheus-formatted text

For Prometheus integration:
1. Get the Prometheus-formatted text via Blueprint
2. Write it to a file or send via network
3. Run a simple HTTP server to expose the file:
   ```
   python -m http.server 9090 --directory /path/to/metrics/
   ```
4. Configure Prometheus to scrape `http://localhost:9090/metrics.txt`

Blueprint Functions
-------------------
The plugin provides these Blueprint functions:

1. `Get Benchmark Metrics` - Returns a struct with all current metrics
2. `Get Prometheus Metrics Text` - Returns metrics in Prometheus text format
3. `Is Prometheus Server Running` - Checks if HTTP server is active
4. `Start Prometheus Server` - Manually start server on specified port
5. `Stop Prometheus Server` - Stop the HTTP server
6. `Get Prometheus Server URL` - Returns the metrics URL

Prometheus Configuration
------------------------
Add this to your Prometheus `prometheus.yml` to read from file:

```yaml
scrape_configs:
  - job_name: 'unreal_benchmarks'
    file_sd_configs:
      - files:
        - '/path/to/your/project/Saved/PrometheusMetrics/metrics.prom'
    scrape_interval: 5s
```

Or use the file_sd_configs to dynamically discover the metrics file.

Grafana Dashboard
-----------------
Import the provided dashboard JSON or create your own using these metrics:
- `unreal_fps` - Frames per second
- `unreal_frame_time` - Frame time in ms
- `unreal_memory_used_physical` - Physical memory used (MB)
- `unreal_cpu_usage` - CPU usage percentage
- `unreal_vertices_total` - Total vertices
- `unreal_triangles_total` - Total triangles
- `unreal_draw_calls` - Draw calls count
- `unreal_gpu_memory_used` - GPU memory used (MB)

Troubleshooting
---------------
- If port 9090 is already in use, the plugin will log a warning
- Metrics collection requires the game/editor to be running
- Some metrics (GPU memory) may be platform-specific
- The HTTP server only runs in packaged builds or PIE

Platform Support
----------------
- Windows (Win64)
- Linux

Requirements
------------
- Unreal Engine 5.5+
- HTTP Server module enabled in project

License
-------
See LICENSE file for details.