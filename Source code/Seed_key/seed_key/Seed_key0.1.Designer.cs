namespace seed_key
{
    partial class Seed_key
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            SK1 = new Label();
            button1 = new Button();
            textBox1 = new TextBox();
            label1 = new Label();
            textBox2 = new TextBox();
            label2 = new Label();
            textBox3 = new TextBox();
            label3 = new Label();
            textBox4 = new TextBox();
            label4 = new Label();
            textBox5 = new TextBox();
            comboBox1 = new ComboBox();
            label5 = new Label();
            SuspendLayout();
            // 
            // SK1
            // 
            SK1.AutoSize = true;
            SK1.Font = new Font("Segoe UI", 9F, FontStyle.Bold, GraphicsUnit.Point);
            SK1.Location = new Point(43, 70);
            SK1.Name = "SK1";
            SK1.Size = new Size(36, 20);
            SK1.TabIndex = 0;
            SK1.Text = "SK1";
            // 
            // button1
            // 
            button1.BackColor = SystemColors.ActiveBorder;
            button1.Font = new Font("Segoe UI", 9F, FontStyle.Bold, GraphicsUnit.Point);
            button1.ForeColor = SystemColors.ControlText;
            button1.Location = new Point(91, 145);
            button1.Name = "button1";
            button1.Size = new Size(125, 48);
            button1.TabIndex = 4;
            button1.Text = "Calculate";
            button1.UseVisualStyleBackColor = false;
            button1.Click += button1_Click;
            // 
            // textBox1
            // 
            textBox1.Location = new Point(25, 28);
            textBox1.Name = "textBox1";
            textBox1.Size = new Size(76, 27);
            textBox1.TabIndex = 0;
            textBox1.TextAlign = HorizontalAlignment.Right;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Font = new Font("Segoe UI", 9F, FontStyle.Bold, GraphicsUnit.Point);
            label1.Location = new Point(158, 70);
            label1.Name = "label1";
            label1.Size = new Size(36, 20);
            label1.TabIndex = 0;
            label1.Text = "SK2";
            // 
            // textBox2
            // 
            textBox2.Location = new Point(140, 28);
            textBox2.Name = "textBox2";
            textBox2.Size = new Size(76, 27);
            textBox2.TabIndex = 1;
            textBox2.TextAlign = HorizontalAlignment.Right;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Font = new Font("Segoe UI", 9F, FontStyle.Bold, GraphicsUnit.Point);
            label2.Location = new Point(275, 70);
            label2.Name = "label2";
            label2.Size = new Size(36, 20);
            label2.TabIndex = 0;
            label2.Text = "SK3";
            // 
            // textBox3
            // 
            textBox3.Location = new Point(257, 28);
            textBox3.Name = "textBox3";
            textBox3.Size = new Size(76, 27);
            textBox3.TabIndex = 2;
            textBox3.TextAlign = HorizontalAlignment.Right;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Font = new Font("Segoe UI", 9F, FontStyle.Bold, GraphicsUnit.Point);
            label3.Location = new Point(406, 70);
            label3.Name = "label3";
            label3.Size = new Size(42, 20);
            label3.TabIndex = 0;
            label3.Text = "Seed";
            // 
            // textBox4
            // 
            textBox4.Location = new Point(388, 28);
            textBox4.Name = "textBox4";
            textBox4.Size = new Size(76, 27);
            textBox4.TabIndex = 3;
            textBox4.TextAlign = HorizontalAlignment.Right;
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Font = new Font("Segoe UI", 9F, FontStyle.Bold, GraphicsUnit.Point);
            label4.Location = new Point(613, 70);
            label4.Name = "label4";
            label4.Size = new Size(35, 20);
            label4.TabIndex = 0;
            label4.Text = "Key";
            // 
            // textBox5
            // 
            textBox5.Location = new Point(582, 28);
            textBox5.Name = "textBox5";
            textBox5.Size = new Size(83, 27);
            textBox5.TabIndex = 10;
            textBox5.TextAlign = HorizontalAlignment.Right;
            // 
            // comboBox1
            // 
            comboBox1.AllowDrop = true;
            comboBox1.FormattingEnabled = true;
            comboBox1.Items.AddRange(new object[] { "Type 1", "Type 2", "Type 3", "Type 4", "Type 5" });
            comboBox1.Location = new Point(406, 156);
            comboBox1.Name = "comboBox1";
            comboBox1.Size = new Size(151, 28);
            comboBox1.TabIndex = 11;
            comboBox1.SelectedIndex = 0;
            // 
            // label5
            // 
            label5.AutoSize = true;
            label5.Location = new Point(558, 229);
            label5.Name = "label5";
            label5.Size = new Size(118, 20);
            label5.TabIndex = 4;
            label5.Text = "Author:Yadunath";
            // 
            // Seed_key
            // 
            AutoScaleDimensions = new SizeF(8F, 20F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(695, 249);
            Controls.Add(label5);
            Controls.Add(comboBox1);
            Controls.Add(textBox5);
            Controls.Add(textBox4);
            Controls.Add(label4);
            Controls.Add(textBox3);
            Controls.Add(label3);
            Controls.Add(textBox2);
            Controls.Add(label2);
            Controls.Add(textBox1);
            Controls.Add(label1);
            Controls.Add(button1);
            Controls.Add(SK1);
            MaximizeBox = false;
            Name = "Seed_key";
            Text = "Seed_key 0.1";
            TopMost = true;
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Label SK1;
        private Button button1;
        private TextBox textBox1;
        private Label label1;
        private TextBox textBox2;
        private Label label2;
        private TextBox textBox3;
        private Label label3;
        private TextBox textBox4;
        private Label label4;
        private TextBox textBox5;
        private ComboBox comboBox1;
        private Label label5;
    }
}