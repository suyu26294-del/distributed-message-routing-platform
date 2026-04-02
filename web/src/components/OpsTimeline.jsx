export default function OpsTimeline({ events, users, fileState }) {
  const steps = [
    { label: 'Alice 登录', done: users.alice.loggedIn },
    { label: 'Bob 登录', done: users.bob.loggedIn },
    { label: '消息实时投递', done: users.bob.messages.some((item) => item.mode === 'live') },
    { label: '离线补偿完成', done: users.bob.messages.some((item) => item.mode === 'offline') },
    { label: '文件续传完成', done: fileState.progressPercent === 100 },
    { label: '下载校验通过', done: fileState.verified },
  ];

  return (
    <section className="ops-layout">
      <div className="card steps-card">
        <div className="panel-header compact">
          <div>
            <p className="eyebrow">Walkthrough</p>
            <h2>演示步骤</h2>
          </div>
        </div>
        <div className="steps-grid">
          {steps.map((step) => (
            <div key={step.label} className={`step-item ${step.done ? 'done' : ''}`}>
              <span>{step.done ? '已完成' : '等待中'}</span>
              <strong>{step.label}</strong>
            </div>
          ))}
        </div>
      </div>
      <div className="card events-card">
        <div className="panel-header compact">
          <div>
            <p className="eyebrow">Event Stream</p>
            <h2>最近事件流</h2>
          </div>
        </div>
        <div className="event-list">
          {events.length === 0 ? <p className="empty-state">事件流会在登录、收发消息、文件上传和系统刷新时实时滚动。</p> : null}
          {events.map((event) => (
            <article key={event.id} className={`event-card ${event.level}`}>
              <header>
                <strong>{event.title}</strong>
                <small>{event.time}</small>
              </header>
              <p>{event.description}</p>
            </article>
          ))}
        </div>
      </div>
    </section>
  );
}