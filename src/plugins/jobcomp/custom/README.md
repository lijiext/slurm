# JobComp Custom JSON Plugin

## 简介
这是一个为 Slurm 开发的自定义作业完成记录（Job Completion）插件。它将作业的终止信息以 **单行 JSON** 格式写入本地文件，专为大数据分析、审计和监控系统设计。

## 核心特性
- **JSON 格式**：每行一个 JSON 对象，包含 JobID, User, State, TRES, QOS 等核心字段。
- **复杂调度支持**：支持作业数组 (Array Jobs)、异构作业 (HetJobs) 和 预约 (Reservations)。
- **健壮性**：自动处理特殊字符转义，防止 JSON 注入；具备错误处理机制，不会因日志写入失败导致 Slurmctld 崩溃。
- **审计友好**：包含 `submit` 时间戳和 `restart_cnt`，能够准确追踪作业的多次重排（requeue）记录。

## 编译与安装

1. **生成构建配置** (如果在 Slurm 源码树中首次添加或修改了目录结构):
   如果源码根目录下有 `autogen.sh`：
   ```bash
   ./autogen.sh
   ```
   如果缺失 `autogen.sh`（常见于官方发布包），请使用：
   ```bash
   autoreconf -vfi
   ```

2. **编译插件**:
   ```bash
   ./configure
   make src/plugins/jobcomp/custom/jobcomp_custom.la
   ```

3. **RPM 打包 (可选)**:
   如果您使用 RPM 进行部署，请在执行完上述步骤后重新打包 Tarball，并运行：
   ```bash
   rpmbuild -ta slurm-custom.tar.bz2
   ```
   插件 `.so` 文件将自动包含在生成的 **`slurm-*.x86_64.rpm`** 主包中。

### DEB 打包 (Ubuntu/Debian) (可选)
如果您在 Ubuntu/Debian 环境下进行打包部署：
1. **准备工作**：同样需要先运行 `autoreconf -vfi` 以确保 `Makefile.in` 已生成。
2. **打包命令**：
   ```bash
   dpkg-buildpackage -b -uc -us
   ```
3. **安装**:
   插件通常会被包含在生成的 **`slurm-client`** 或 **`libslurm`** 相关的 `.deb` 包中。安装后可通过以下命令确认：
   ```bash
   dpkg -L <package_name> | grep jobcomp_custom.so
   ```

4. **安装**:
   - **手动安装**: 将生成的 `.so` 文件复制到 `/usr/lib64/slurm/`。
   - **RPM 安装**: 正常安装/更新生成的 `slurm` RPM 包即可。

## 配置方法

修改 `slurm.conf` 配置文件：

```ini
# 指定使用自定义 JSON 插件
JobCompType=jobcomp/custom

# 指定日志保存路径
JobCompLoc=/var/log/slurm/job_completion.json
```

修改完成后，执行以下命令使配置生效：
```bash
scontrol reconfigure
```

## 输出字段说明
- `job_id`: 任务的唯一标识 ID。
- `array_job_id / array_task_id`: 作业数组的主 ID 和任务索引。
- `het_job_id / het_job_offset`: 异构作业的 ID 和组件偏移量。
- `elapsed_raw`: 作业运行的总时长（秒）。
- `cpu_time_raw`: 消耗的总 CPU 时间（核秒）。
- `submit / start / end`: 提交、开始、结束的 Unix 时间戳。
- `tres`: 分配的资源字符串（如 cpu=1,mem=100M）。
- `restart_cnt`: 作业重启/重排的次数。
