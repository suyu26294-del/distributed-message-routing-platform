import "./styles/dashboard.css";

type NodeCard = {
  nodeId: string;
  service: string;
  load: number;
  status: string;
};

type FileFlow = {
  fileId: string;
  chunkCount: number;
  hotHitRate: string;
  resumePoint: string;
};

const nodes: NodeCard[] = [
  { nodeId: "gateway-a", service: "gateway", load: 26.5, status: "healthy" },
  { nodeId: "message-b", service: "message-service", load: 13.2, status: "healthy" },
  { nodeId: "file-a", service: "file-service", load: 11.8, status: "healthy" },
  { nodeId: "scheduler-a", service: "scheduler", load: 6.1, status: "healthy" }
];

const fileFlows: FileFlow[] = [
  { fileId: "file-1", chunkCount: 2, hotHitRate: "50%", resumePoint: "chunk 1" },
  { fileId: "file-2", chunkCount: 6, hotHitRate: "83%", resumePoint: "complete" }
];

const routeHeat = [
  { key: "user:1000", target: "message-b" },
  { key: "user:1001", target: "message-b" },
  { key: "file:file-1", target: "file-a" },
  { key: "file:file-2", target: "file-a" }
];

function App() {
  return (
    <div className="app-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">Distributed Platform V1</p>
          <h1>消息路由与文件分发控制台</h1>
          <p className="subtitle">
            展示节点状态、消息离线补发、文件分块缓存与断点续传的最小演示后台。
          </p>
        </div>
        <div className="hero-metrics">
          <div>
            <span>在线节点</span>
            <strong>4</strong>
          </div>
          <div>
            <span>离线补发</span>
            <strong>1</strong>
          </div>
          <div>
            <span>热点命中</span>
            <strong>67%</strong>
          </div>
        </div>
      </header>

      <main className="grid">
        <section className="panel panel-wide">
          <div className="panel-header">
            <h2>集群拓扑</h2>
            <span>一致性哈希 + 负载评分</span>
          </div>
          <div className="node-grid">
            {nodes.map((node) => (
              <article key={node.nodeId} className="node-card">
                <span className="badge">{node.service}</span>
                <h3>{node.nodeId}</h3>
                <p>load score: {node.load.toFixed(1)}</p>
                <p>status: {node.status}</p>
              </article>
            ))}
          </div>
        </section>

        <section className="panel">
          <div className="panel-header">
            <h2>消息链路</h2>
            <span>离线存储与未读计数</span>
          </div>
          <ol className="timeline">
            <li>u1000 发送消息到 u1001，Scheduler 选到 message-b</li>
            <li>u1001 不在线，消息写入离线队列，未读数 +1</li>
            <li>u1001 登录后执行 PullOffline，系统补发并清零未读</li>
          </ol>
        </section>

        <section className="panel">
          <div className="panel-header">
            <h2>文件链路</h2>
            <span>Chunk / Resume / Cache</span>
          </div>
          <div className="file-list">
            {fileFlows.map((flow) => (
              <div key={flow.fileId} className="file-row">
                <strong>{flow.fileId}</strong>
                <span>{flow.chunkCount} chunks</span>
                <span>cache {flow.hotHitRate}</span>
                <span>{flow.resumePoint}</span>
              </div>
            ))}
          </div>
        </section>

        <section className="panel panel-wide">
          <div className="panel-header">
            <h2>缓存与路由</h2>
            <span>路由热区示意</span>
          </div>
          <table className="route-table">
            <thead>
              <tr>
                <th>route key</th>
                <th>resolved node</th>
              </tr>
            </thead>
            <tbody>
              {routeHeat.map((item) => (
                <tr key={item.key}>
                  <td>{item.key}</td>
                  <td>{item.target}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </section>
      </main>
    </div>
  );
}

export default App;

