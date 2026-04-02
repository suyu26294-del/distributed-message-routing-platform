export default function FileConsole({ fileState, onChooseFile, onUseSampleFile, onCreateManifest, onUploadFirstChunk, onResumeUpload, onDownloadVerify }) {
  return (
    <section className="panel-shell">
      <div className="panel-header">
        <div>
          <p className="eyebrow">File Routing</p>
          <h2>分片上传与断点续传</h2>
        </div>
        <p className="panel-copy">支持本地选择文件，也可以一键生成示例文件。首片暂停后再恢复续传，方便展示断点续传路径。</p>
      </div>
      <div className="file-card card">
        <div className="file-cta-row">
          <label className="file-picker">
            <input type="file" onChange={onChooseFile} />
            <span>选择本地文件</span>
          </label>
          <button className="secondary-button" onClick={onUseSampleFile}>生成示例文件</button>
        </div>
        <div className="file-stats-grid">
          <div>
            <span>文件名</span>
            <strong>{fileState.fileName || '未选择'}</strong>
          </div>
          <div>
            <span>文件大小</span>
            <strong>{fileState.fileSize ? `${fileState.fileSize} bytes` : '--'}</strong>
          </div>
          <div>
            <span>分片数量</span>
            <strong>{fileState.totalChunks || '--'}</strong>
          </div>
          <div>
            <span>传输 ID</span>
            <strong>{fileState.transferId || '--'}</strong>
          </div>
        </div>
        <div className="progress-shell">
          <div className="progress-bar">
            <div className="progress-fill" style={{ width: `${fileState.progressPercent}%` }} />
          </div>
          <span>{fileState.progressPercent}% 已完成</span>
        </div>
        <div className="tag-row">
          <span className="data-tag">chunk size {fileState.chunkSize}</span>
          <span className="data-tag">missing {fileState.missingChunks.join(', ') || 'none'}</span>
          <span className={`data-tag ${fileState.verified ? 'verified-tag' : ''}`}>{fileState.verified ? '下载校验通过' : '等待校验'}</span>
        </div>
        <div className="file-actions">
          <button className="secondary-button" onClick={onCreateManifest}>创建传输</button>
          <button className="secondary-button" onClick={onUploadFirstChunk}>上传首片并暂停</button>
          <button className="primary-button" onClick={onResumeUpload}>恢复续传</button>
          <button className="secondary-button" onClick={onDownloadVerify}>下载并校验</button>
        </div>
      </div>
    </section>
  );
}