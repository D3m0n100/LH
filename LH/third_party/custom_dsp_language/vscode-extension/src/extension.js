const vscode = require('vscode');
const { spawn } = require('child_process');
const path = require('path');

function activate(context) {
    let disposable = vscode.commands.registerCommand('custom-dsp.compile', () => {
        const editor = vscode.window.activeTextEditor;
        
        if (editor && editor.document.languageId === 'custom-dsp') {
            const sourceCode = editor.document.getText();
            compileCode(sourceCode);
        } else {
            vscode.window.showErrorMessage('请打开一个.cdsp文件');
        }
    });

    context.subscriptions.push(disposable);
}

function compileCode(sourceCode) {
    // 获取编译器路径
    const compilerPath = path.join(__dirname, '../../compile/compiler.py');
    
    // 运行Python编译器
    const python = spawn('python', [compilerPath], {
        stdio: ['pipe', 'pipe', 'pipe']
    });

    python.stdin.write(sourceCode);
    python.stdin.end();

    let output = '';
    python.stdout.on('data', (data) => {
        output += data.toString();
    });

    python.stderr.on('data', (data) => {
        vscode.window.showErrorMessage(`编译错误: ${data}`);
    });

    python.on('close', (code) => {
        if (code === 0) {
            // 在新窗口显示结果
            vscode.workspace.openTextDocument({
                content: output,
                language: 'text'
            }).then(doc => {
                vscode.window.showTextDocument(doc);
            });
        }
    });
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
};