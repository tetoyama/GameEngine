using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

// C++から呼び出すスクリプト基底クラス（ScriptWrapperでcastする）
public class PlayerScript : ScriptBase
{
    public override void Start()
    {
        Console.WriteLine("PlayerScript: Start()");
    }

    public override void Update(float dt)
    {
        Console.WriteLine($"PlayerScript: Update({dt})");
    }

    public override void FixedUpdate(float dt)
    {
        Console.WriteLine($"PlayerScript: FixedUpdate({dt})");
    }
    public override void Draw()
    {
        Console.WriteLine($"PlayerScript: Draw()");
    }

    public override void Stop()
    {
        Console.WriteLine("PlayerScript: Stop()");
    }
}

