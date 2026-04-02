function UserCard({ userId, state, onLogin, onDisconnect, onPullOffline }) {
  return (
    <div className="user-card card">
      <div className="user-header">
        <div>
          <p className="eyebrow">Demo Identity</p>
          <h2>{userId}</h2>
        </div>
        <span className={`status-badge ${state.loggedIn ? 'healthy' : 'unhealthy'}`}>
          {state.loggedIn ? '在线' : '离线'}
        </span>
      </div>
      <div className="user-actions">
        <button className="secondary-button" onClick={() => onLogin(userId)}>登录</button>
        <button className="secondary-button ghost-button" onClick={() => onDisconnect(userId)}>断开</button>
        <button className="secondary-button" onClick={() => onPullOffline(userId)}>拉取离线</button>
      </div>
      <p className="session-note">session: {state.sessionId || '未建立'}</p>
      <div className="message-list">
        {state.messages.length === 0 ? <p className="empty-state">还没有消息，先试试登录和发送消息。</p> : null}
        {state.messages.map((message) => (
          <article key={message.id} className={`message-bubble ${message.direction}`}>
            <header>
              <span>{message.from}{' -> '}{message.to}</span>
              <small>{message.mode}</small>
            </header>
            <p>{message.body}</p>
          </article>
        ))}
      </div>
    </div>
  );
}

export default function MessageConsole({ users, compose, onComposeChange, onSendMessage, onLogin, onDisconnect, onPullOffline }) {
  return (
    <section className="panel-shell">
      <div className="panel-header">
        <div>
          <p className="eyebrow">Messaging</p>
          <h2>双端会话演示</h2>
        </div>
        <p className="panel-copy">这里保留 Alice 和 Bob 两个视角，既能看实时投递，也能主动模拟离线补偿。</p>
      </div>
      <div className="message-layout">
        <UserCard userId="alice" state={users.alice} onLogin={onLogin} onDisconnect={onDisconnect} onPullOffline={onPullOffline} />
        <UserCard userId="bob" state={users.bob} onLogin={onLogin} onDisconnect={onDisconnect} onPullOffline={onPullOffline} />
      </div>
      <div className="composer card">
        <div className="composer-row">
          <label>
            发送方
            <select value={compose.from_user} onChange={(event) => onComposeChange({ ...compose, from_user: event.target.value })}>
              <option value="alice">alice</option>
              <option value="bob">bob</option>
            </select>
          </label>
          <label>
            接收方
            <select value={compose.to_user} onChange={(event) => onComposeChange({ ...compose, to_user: event.target.value })}>
              <option value="bob">bob</option>
              <option value="alice">alice</option>
            </select>
          </label>
        </div>
        <label className="full-width">
          消息内容
          <textarea value={compose.body} onChange={(event) => onComposeChange({ ...compose, body: event.target.value })} rows="3" />
        </label>
        <button className="primary-button" onClick={onSendMessage}>发送消息</button>
      </div>
    </section>
  );
}