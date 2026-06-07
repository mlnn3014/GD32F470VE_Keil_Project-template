using System;
using System.IO;
using System.Reflection;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;
using DataSample4CIMC.AutoReview;

// .autosp -> .csv 转换器
// 直接复用原程序自带的 AutoReviewReport.Load + ExportToCsv，产出与官方逐字节一致的 CSV。
// 用法: Autosp2Csv <报告.autosp> [输出.csv]
internal static class Autosp2Csv
{
    // 把序列化流里的 DataSample4CIMC 类型绑到本地随附的程序集（忽略版本/PublicKeyToken 差异，
    // 这样不同小版本导出的 .autosp 也能读）。
    private sealed class LocalBinder : SerializationBinder
    {
        private readonly Assembly _asm;
        public LocalBinder(Assembly asm) { _asm = asm; }
        public override Type BindToType(string assemblyName, string typeName)
        {
            Type t = _asm.GetType(typeName);
            return t ?? Type.GetType(typeName); // 返回 null 时 BinaryFormatter 走默认解析(框架类型)
        }
    }

    private static int Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.WriteLine("用法: Autosp2Csv <报告.autosp> [输出.csv]");
            return 1;
        }
        string inp = args[0];
        if (!File.Exists(inp)) { Console.WriteLine("找不到文件: " + inp); return 1; }
        string outp = args.Length > 1 ? args[1] : Path.ChangeExtension(inp, ".csv");

        AutoReviewReport rep;
        var bf = new BinaryFormatter { Binder = new LocalBinder(typeof(AutoReviewReport).Assembly) };
        using (var fs = new FileStream(inp, FileMode.Open, FileAccess.Read, FileShare.Read))
            rep = (AutoReviewReport)bf.Deserialize(fs);

        rep.ExportToCsv(outp); // 原程序自带的导出逻辑

        Console.WriteLine(string.Format("队伍={0}  总分={1:F1}  记录={2} 条  NTP={3}",
            rep.TeamNumber, rep.TotalScore, rep.Records.Count, rep.NtpIsSynced ? "已同步" : "未同步"));
        Console.WriteLine("CSV(UTF-8) -> " + outp);
        return 0;
    }
}
