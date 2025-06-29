using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClassLibrary
{
    public abstract class ScriptBase
    {
        public virtual void Start() { }
        public virtual void Update(float dt) { }
        public virtual void FixedUpdate(float dt) { }
        public virtual void Stop() { }
    }

}
